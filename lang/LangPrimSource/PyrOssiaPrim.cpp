#include "PyrOssiaPrim.h"

#include <boost/bind.hpp>
#include <iostream>
#include <boost/thread.hpp>

#define STRMAXLE 4096

extern bool compiledOK;

using boost::asio::ip::tcp;
using namespace ossia::supercollider;
using pointer = boost::shared_ptr<tcp_connection>;
extern boost::asio::io_context ioService;
#define errpost_return(err) std::cout << err.message(); return;

template<> inline float ossia::supercollider::read(pyrslot* s)
{
    float f; slotFloatVal(s, &f); return f;
}

template<> inline int ossia::supercollider::read(pyrslot* s)
{
    int i; slotIntVal(s, &i); return i;
}

template<> inline bool ossia::supercollider::read(pyrslot* s)
{
    return IsTrue(s);
}

template<> inline std::string ossia::supercollider::read(pyrslot* s)
{
    char v[ STRMAXLE ]; slotStrVal(s, v, STRMAXLE);
    return static_cast<std::string>(v);
}

template<> inline void ossia::supercollider::write(pyrslot* s, int v)
{
    SetInt(s, v);
}

template<> inline void ossia::supercollider::write(pyrslot* s, float v)
{
    SetFloat(s, v);
}

template<> inline void ossia::supercollider::write(pyrslot* s, std::string v)
{
    PyrString* str = newPyrString(gMainVMGlobals->gc, v.c_str(), 0, true);
    SetObject(s, str);
}

template<> inline void ossia::supercollider::write(pyrslot* s, void* v)
{
    SetPtr(s, v);
}

template<> inline void ossia::supercollider::write(pyrslot* s, bool v)
{
    SetBool(s, v);
}

template<typename T> inline void ossia::supercollider::register_object(
        pyrslot* s, T* object, uint16_t v_index )
{
    pyrslot* ivar = slotRawObject(s)->slots+v_index;
    SetPtr(  ivar, object );
}

template<typename T> inline T* ossia::supercollider::get_object(pyrslot* s, uint16_t v_index)
{
    return static_cast<T*>(slotRawPtr(&slotRawObject(s)->slots[v_index]));
}

template<typename T> void ossia::supercollider::sendback_object(
        pyrobject* object, T* pointer, const char* sym )
{
    gLangMutex.lock();

    if ( compiledOK )
    {
        auto g = gMainVMGlobals;
        g->canCallOS = true;

        ++g->sp; SetObject(g->sp, object);
        ++g->sp; SetPtr(g->sp, pointer);
        runInterpreter(g, getsym(sym), 2);

        g->canCallOS = false;
    }

    gLangMutex.unlock();

}

inline void ossia::supercollider::free(vmglobals* g, pyrslot* s)
{
    g->gc->Free(slotRawObject(s));
    SetNil(s);
}

//----------------------------------------------------------------------------------- CONNECTIONS

pointer tcp_connection::create(boost::asio::io_context& io_context)
{
    return pointer( new tcp_connection(io_context) );
}

tcp_connection::tcp_connection( boost::asio::io_context& ctx ) :
    m_socket(ctx) { }

void tcp_connection::bind(pyrobject* object)
{
    m_object = object;
}

inline std::string tcp_connection::remote_address() const
{
    return m_socket.remote_endpoint().address().to_v4().to_string();
}

inline uint16_t tcp_connection::remote_port() const
{
    return m_socket.remote_endpoint().port();
}

void tcp_connection::listen()
{    
    boost::asio::async_read(m_socket, boost::asio::buffer(m_netbuffer),
        boost::asio::transfer_at_least(1),
        boost::bind(&tcp_connection::read_handler, shared_from_this(),
        boost::asio::placeholders::error,
        boost::asio::placeholders::bytes_transferred));
}

void tcp_connection::read_handler(const boost::system::error_code& err, size_t nbytes)
{        
    if ( err ) { errpost_return(err); }
    std::string chunk = m_netbuffer.data();

    gLangMutex.lock();

    if ( compiledOK )
    {
        auto g = gMainVMGlobals;
        g->canCallOS = true;

        ++g->sp; SetObject(g->sp, m_object);
        ++g->sp; ossia::supercollider::write(g->sp, chunk);
        runInterpreter(g, getsym("onDataReceived"), 2);

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
    if ( err ) { errpost_return(err); }
    std::cout << "nbytes written: " << std::to_string(nbytes) << std::endl;
}

//----------------------------------------------------------------------------------- SERVER

tcp_server* tcp_server::create(uint16_t port, pyrslot* s)
{
    return new tcp_server(ioService, port, s);
}

tcp_server::tcp_server(boost::asio::io_context& ctx, uint16_t port, pyrslot *s) :
    m_ctx(ctx), m_acceptor(ctx, tcp::endpoint(tcp::v4(), port))
{
    m_object = slotRawObject(s);
    register_object<tcp_server>(s, this, 0);

    start_accept();
}

tcp_server::~tcp_server()
{
    for ( auto& connection : m_connections )
          connection.reset();
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
    if ( err ) { errpost_return(err); }

    connection->listen();
    sendback_object<tcp_connection>(
        m_object, connection.get(), "onNewConnection");

    m_connections.push_back( connection );
    start_accept();
}

//----------------------------------------------------------------------------------- CLIENT

tcp_client* tcp_client::create(pyrslot* s)
{
    return new tcp_client(ioService, s);
}

tcp_client::tcp_client(boost::asio::io_context& ctx, pyrslot *s) : m_ctx(ctx)
{
    m_object = slotRawObject(s);
    register_object<tcp_client>(s, this, 0);
}

tcp_client::~tcp_client()
{
    m_connection.reset();
}

void tcp_client::connect( std::string const& host_addr, uint16_t port )
{
    boost::asio::ip::tcp::endpoint endpoint(
                boost::asio::ip::address::from_string(host_addr), port );

    m_connection = tcp_connection::create(m_ctx);
    auto& socket = m_connection->socket();

    socket.async_connect(endpoint,
        boost::bind(&tcp_client::connected_handler, this,
        m_connection, boost::asio::placeholders::error));
}

void tcp_client::connected_handler(
        tcp_connection::pointer connection, const boost::system::error_code& err )
{
    if ( err ) { errpost_return(err); }
    m_connection->listen();

    sendback_object<tcp_connection>(
        m_object, m_connection.get(), "onConnected");
}

//----------------------------------------------------------------------------------- PRIMITIVES

int pyr_tcp_server_instantiate_run(VMGlobals* g, int n)
{
    tcp_server::create(read<int>(g->sp), g->sp-1);
    return errNone;
}

int pyr_tcp_server_free(VMGlobals* g, int n)
{
    delete get_object<tcp_server>(g->sp, 0);
    free( g, g->sp );

    return errNone;
}

int pyr_tcp_client_instantiate(VMGlobals* g, int n)
{
    tcp_client::create(g->sp);
    return errNone;
}

int pyr_tcp_client_free(VMGlobals* g, int n)
{
    delete get_object<tcp_client>(g->sp, 0);
    free( g, g->sp );

    return errNone;
}

int pyr_tcp_client_connect(VMGlobals* g, int n)
{
    auto client = get_object<tcp_client>(g->sp-2, 0);
    client->connect(read<std::string>(g->sp-1), read<int>(g->sp));

    return errNone;
}

int pyr_tcp_client_disconnect(VMGlobals* g, int n)
{
    return errNone;
}

int pyr_tcp_con_bind(VMGlobals* g, int n)
{
    auto connection = get_object<tcp_connection>(g->sp, 0);
    connection->bind(slotRawObject(g->sp));

    return errNone;
}

int pyr_tcp_con_write(VMGlobals* g, int n)
{
    auto connection = get_object<tcp_connection>(g->sp-1, 0);
    connection->write(read<std::string>(g->sp));

    return errNone;
}

int pyr_tcp_con_write_ws(VMGlobals* g, int n)
{
    return errNone;
}

int pyr_tcp_con_get_remote_address(VMGlobals* g, int n)
{
    auto connection = get_object<tcp_connection>(g->sp, 0);
    auto addr = connection->remote_address();
    write<std::string>(g->sp, addr);

    return errNone;
}

int pyr_tcp_con_get_remote_port(VMGlobals* g, int n)
{
    auto connection = get_object<tcp_connection>(g->sp, 0);
    auto port = connection->remote_port();
    write<int>(g->sp, port);

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

    definePrimitive( base, index++, "_TcpConnectionBind", pyr_tcp_con_bind, 1, 0);
    definePrimitive( base, index++, "_TcpConnectionWrite", pyr_tcp_con_write, 2, 0);
    definePrimitive( base ,index++, "_TcpConnectionWriteWebSocket", pyr_tcp_con_write_ws, 3, 0);
    definePrimitive( base, index++, "_TcpConnectionGetRemoteAddress", pyr_tcp_con_get_remote_address, 1, 0);
    definePrimitive( base, index++, "_TcpConnectionGetRemotePort", pyr_tcp_con_get_remote_port, 1, 0);

}
