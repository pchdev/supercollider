#include "PyrOssiaPrim.h"

#include <boost/bind.hpp>
#include <iostream>

#define STRMAXLE 4096
extern bool compiledOK;

using boost::asio::ip::tcp;
using namespace ossia::supercollider;
using pointer = boost::shared_ptr<tcp_connection>;

boost::asio::io_context g_ioctx { };
std::thread* g_iothread;

inline float ossia::supercollider::read_float(pyrslot* s)
{
    float f; slotFloatVal(s, &f);
    return f;
}

inline int ossia::supercollider::read_int(pyrslot* s)
{
    int i; slotIntVal(s, &i);
    return i;
}

inline std::string ossia::supercollider::read_string(pyrslot* s)
{
    char v[ STRMAXLE ]; slotStrVal(s, v, STRMAXLE);
    return static_cast<std::string>(v);
}

template<typename T> inline T* ossia::supercollider::get_object(pyrslot* s, uint16_t v_index)
{
    return static_cast<T*>(slotRawPtr(&slotRawObject(s)->slots[v_index]));
}

//----------------------------------------------------------------------------------- CONNECTIONS

pointer tcp_connection::create(boost::asio::io_context& io_context)
{
    return pointer( new tcp_connection(io_context) );
}

tcp_connection::tcp_connection( boost::asio::io_context& ctx ) : m_socket(ctx)
{

}

void tcp_connection::listen()
{
    boost::asio::async_read(m_socket, boost::asio::buffer(m_netbuffer),
        boost::asio::transfer_all(),
        boost::bind(&tcp_connection::read_handler, shared_from_this(),
        boost::asio::placeholders::error,
        boost::asio::placeholders::bytes_transferred));
}

void tcp_connection::read_handler(const boost::system::error_code& err, size_t nbytes)
{
    listen();
}

void tcp_connection::write(const std::string& data)
{
    auto buf = boost::asio::buffer( data );

    boost::asio::async_write( m_socket, buf,
        boost::bind( &tcp_connection::write_handler, shared_from_this(),
        boost::asio::placeholders::error,
        boost::asio::placeholders::bytes_transferred ) );
}

void tcp_connection::write_handler(const boost::system::error_code& err, size_t nbytes)
{

}

//----------------------------------------------------------------------------------- SERVER

tcp_server::tcp_server(boost::asio::io_context& ctx, uint16_t port, pyrslot *s) :
    m_acceptor(ctx, tcp::endpoint(tcp::v4(), port))
{
    SetPtr(s, this);
    m_object = slotRawObject(s);

    start_accept();
}

void tcp_server::start_accept()
{
    pointer new_connection = tcp_connection::create( g_ioctx );

    std::cout << "server: starting accept" << std::endl;
    m_acceptor.async_accept( new_connection->socket(),
        boost::bind(&tcp_server::accept_handler, this, new_connection,
        boost::asio::placeholders::error ) );
}

void tcp_server::accept_handler( tcp_connection::pointer connection,
                                const boost::system::error_code& err )
{
    std::cout << "server: new_connection" << std::endl;

    if ( !err ) m_connections.push_back( connection );
    start_accept();        

    auto g = &gVMGlobals;
    gLangMutex.lock();

    if ( compiledOK )
    {
        g->canCallOS = true;

        ++g->sp; SetObject(g->sp, m_object);
        ++g->sp; SetPtr(g->sp, connection.get());
        runInterpreter(g, getsym("onNewConnection"), 2);

        g->canCallOS = false;
    }

    gLangMutex.unlock();
}

//----------------------------------------------------------------------------------- CLIENT

tcp_client::tcp_client(boost::asio::io_context& ctx, pyrslot *s)
{
    SetPtr(s, this);
    m_object = slotRawObject(s);
}

void tcp_client::connect( std::string const& host_addr, uint16_t port )
{
    boost::asio::ip::tcp::endpoint endpoint(
                boost::asio::ip::address::from_string(host_addr), port );

    auto connection = tcp_connection::create(g_ioctx);
    auto& socket = connection->socket();

    std::cout << "client: attempting connection: "
              << endpoint.address().to_string()
              << ":"
              << std::to_string(endpoint.port())
              << std::endl;

    socket.async_connect(endpoint,
        boost::bind(&tcp_client::connected_handler, this,
        connection, boost::asio::placeholders::error));
}

void tcp_client::connected_handler(tcp_connection::pointer con, const boost::system::error_code& err )
{
    con->listen();
    m_connection = con;

    std::cout << "client connected" << std::endl;

    auto g = &gVMGlobals;
    gLangMutex.lock();

    if ( compiledOK )
    {
        g->canCallOS = true;

        ++g->sp; SetObject(g->sp, m_object);
        ++g->sp; SetPtr(g->sp, con.get());
        runInterpreter(g, getsym("onConnected"), 2);

        g->canCallOS = false;
    }

    gLangMutex.unlock();
}

void tcp_client::write( const std::string& data)
{
    m_connection->write(data);
}

//----------------------------------------------------------------------------------- PRIMITIVES

int pyr_tcp_server_instantiate_run(VMGlobals* g, int n)
{
    auto server = new tcp_server( g_ioctx, read_int(g->sp), g->sp-1);
    return errNone;
}

int pyr_tcp_server_get_new_connection(VMGlobals* g, int n)
{
    return errNone;
}

int pyr_tcp_server_get_disconnection(VMGlobals* g, int n)
{
    return errNone;
}

int pyr_tcp_server_get_connection_at(VMGlobals* g, int n)
{
    return errNone;
}

int pyr_tcp_client_instantiate(VMGlobals* g, int n)
{
    auto client = new tcp_client( g_ioctx, g->sp);
    return errNone;
}

int pyr_tcp_client_connect(VMGlobals* g, int n)
{
    auto client = get_object<tcp_client>(g->sp-2, 0);
    client->connect(read_string(g->sp-1), read_int(g->sp));

    return errNone;
}

int pyr_tcp_client_disconnect(VMGlobals* g, int n)
{
    return errNone;
}

int pyr_tcp_con_write(VMGlobals* g, int n)
{
    auto connection = get_object<tcp_connection>(g->sp-1, 0);
    connection->write(read_string(g->sp));

    return errNone;
}

void ossia::supercollider::initialize()
{
    int base, index = 0;
    base = nextPrimitiveIndex();    

    definePrimitive( base, index++, "_TcpServerInstantiateRun", pyr_tcp_server_instantiate_run, 2, 0 );
    definePrimitive( base, index++, "_TcpServerGetConnectionAt", pyr_tcp_server_get_connection_at, 2, 0 );
    definePrimitive( base, index++, "_TcpServerGetNewConnection", pyr_tcp_server_get_new_connection, 2, 0 );
    definePrimitive( base, index++, "_TcpServerGetDisconnection", pyr_tcp_server_get_disconnection, 2, 0 );

    definePrimitive( base, index++, "_TcpClientInstantiate", pyr_tcp_client_instantiate, 2, 0);
    definePrimitive( base, index++, "_TcpClientConnect", pyr_tcp_client_connect, 3, 0);
    definePrimitive( base, index++, "_TcpClientDisconnect", pyr_tcp_client_disconnect, 2, 0);

    definePrimitive( base, index++, "_TcpConnectionWrite", pyr_tcp_con_write, 2, 0);
}
