#include "PyrOssiaPrim.h"

#include <boost/bind.hpp>
#include <iostream>
#include <boost/thread.hpp>

#define STRMAXLE 4096
extern bool compiledOK;

using boost::asio::ip::tcp;
using namespace ossia::supercollider;
using pointer = boost::shared_ptr<tcp_connection>;

extern boost::asio::io_service ioService;

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

void tcp_connection::bind(pyrobject* object)
{
    m_object = object;
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
    auto g = gMainVMGlobals;
    gLangMutex.lock();

    if ( !compiledOK ) // !TODO
    {
        g->canCallOS = true;

        ++g->sp; SetObject(g->sp, m_object);
        // TODO: send data back
        runInterpreter(g, getsym("onDataReceived"), 1);

        g->canCallOS = false;
    }

    gLangMutex.unlock();
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

tcp_server* tcp_server::create(uint16_t port, pyrslot* s)
{
    return new tcp_server(ioService, port, s);
}

tcp_server::tcp_server(boost::asio::io_context& ctx, uint16_t port, pyrslot *s) :
    m_ctx(ctx), m_acceptor(ctx, tcp::endpoint(tcp::v4(), port))
{
    SetPtr(s, this);
    m_object = slotRawObject(s);
}

tcp_server::~tcp_server()
{

}

void tcp_server::run()
{
    start_accept();
}

void tcp_server::start_accept()
{
    pointer new_connection = tcp_connection::create( m_ctx );

    m_acceptor.async_accept( new_connection->socket(),
        boost::bind(&tcp_server::accept_handler, this, new_connection,
        boost::asio::placeholders::error ) );
}

void tcp_server::accept_handler( tcp_connection::pointer connection,
                                const boost::system::error_code& err )
{
    std::cout << "server: new_connection" << std::endl;

    if ( !err ) m_connections.push_back( connection );

    auto g = gMainVMGlobals;
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
    start_accept();
}

//----------------------------------------------------------------------------------- CLIENT

tcp_client* tcp_client::create(pyrslot* s)
{
    return new tcp_client(ioService, s);
}

tcp_client::tcp_client(boost::asio::io_context& ctx, pyrslot *s) : m_ctx(ctx)
{
    SetPtr(s, this);
    m_object = slotRawObject(s);
}

tcp_client::~tcp_client()
{

}

void tcp_client::connect( std::string const& host_addr, uint16_t port )
{
    boost::asio::ip::tcp::endpoint endpoint(
                boost::asio::ip::address::from_string(host_addr), port );

    auto connection = tcp_connection::create(m_ctx);
    auto& socket = connection->socket();

    socket.async_connect(endpoint,
        boost::bind(&tcp_client::connected_handler, this,
        connection, boost::asio::placeholders::error));
}

void tcp_client::connected_handler(tcp_connection::pointer con, const boost::system::error_code& err )
{
    std::cout << err.message();

    con->listen();
    m_connection = con;

    auto g = gMainVMGlobals;
    gLangMutex.lock();

//    if ( !compiledOK )
//    {
//        g->canCallOS = true;

//        ++g->sp; SetObject(g->sp, m_object);
//        ++g->sp; SetPtr(g->sp, con.get());
//        runInterpreter(g, getsym("onConnected"), 2);

//        g->canCallOS = false;
//    }

    gLangMutex.unlock();
}

void tcp_client::write( const std::string& data)
{
    m_connection->write(data);
}

//----------------------------------------------------------------------------------- PRIMITIVES

int pyr_tcp_server_instantiate_run(VMGlobals* g, int n)
{
    auto server = tcp_server::create(read_int(g->sp), g->sp-1);
    server->run();

    return errNone;
}

int pyr_tcp_server_free(VMGlobals* g, int n)
{
    auto server = get_object<tcp_server>(g->sp, 0);
    delete server;

    auto device_obj = slotRawObject(g->sp);
    g->gc->Free(device_obj);
    SetNil(g->sp);
}

int pyr_tcp_client_instantiate(VMGlobals* g, int n)
{
    auto client = tcp_client::create(g->sp);
    return errNone;
}

int pyr_tcp_client_free(VMGlobals* g, int n)
{
    auto client = get_object<tcp_client>(g->sp, 0);
    delete client;

    auto device_obj = slotRawObject(g->sp);
    g->gc->Free(device_obj);
    SetNil(g->sp);
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

int pyr_tcp_con_bind(VMGlobals* g, int n)
{
    return errNone;
}

int pyr_tcp_con_write(VMGlobals* g, int n)
{
    auto connection = get_object<tcp_connection>(g->sp-1, 0);
    connection->write(read_string(g->sp));

    return errNone;
}

int pyr_tcp_con_write_ws(VMGlobals* g, int n)
{
    return errNone;
}

void ossia::supercollider::initialize()
{
    int base, index = 0;
    base = nextPrimitiveIndex();    

    definePrimitive( base, index++, "_TcpServerInstantiateRun", pyr_tcp_server_instantiate_run, 2, 0 );
    definePrimitive( base, index++, "_TcpServerFree", pyr_tcp_server_free, 1, 0);

    definePrimitive( base, index++, "_TcpClientInstantiate", pyr_tcp_client_instantiate, 1, 0);
    definePrimitive( base, index++, "_TcpClientConnect", pyr_tcp_client_connect, 3, 0);
    definePrimitive( base, index++, "_TcpClientDisconnect", pyr_tcp_client_disconnect, 2, 0);
    definePrimitive( base, index++, "_TcpClientFree", pyr_tcp_client_free, 1, 0);

    definePrimitive( base, index++, "_TcpConnectionBind", pyr_tcp_con_bind, 2, 0);
    definePrimitive( base, index++, "_TcpConnectionWrite", pyr_tcp_con_write, 2, 0);
    definePrimitive( base ,index++, "_TcpConnectionWriteWebSocket", pyr_tcp_con_write_ws, 3, 0);
}
