#include "PyrOssiaPrim.h"

#include <boost/bind.hpp>
#include <iostream>
#include <boost/thread.hpp>
#include <QCryptographicHash>

#define STRMAXLE 4096
#define errpost_return(err) std::cout << err.message() << std::endl; return;

extern bool compiledOK;
extern boost::asio::io_context ioService;

using boost::asio::ip::tcp;
using namespace ossia::supercollider;
using pointer = boost::shared_ptr<tcp_connection>;

template<> inline bool ossia::supercollider::read( pyrslot* s) { return IsTrue( s ); }
template<> inline float ossia::supercollider::read(pyrslot* s)
{
    float f; slotFloatVal(s, &f); return f;
}

template<> inline int ossia::supercollider::read(pyrslot* s)
{
    int i; slotIntVal(s, &i); return i;
}

template<> inline std::string ossia::supercollider::read(pyrslot* s)
{
    char v[ STRMAXLE ]; slotStrVal(s, v, STRMAXLE);
    return static_cast<std::string>(v);
}

template<> inline void ossia::supercollider::write(pyrslot* s, int v)    { SetInt    ( s, v );   }
template<> inline void ossia::supercollider::write(pyrslot* s, float v)  { SetFloat  ( s, v );   }
template<> inline void ossia::supercollider::write(pyrslot* s, double v) { SetFloat  ( s, v );   }
template<> inline void ossia::supercollider::write(pyrslot* s, void* v)  { SetPtr    ( s, v );   }
template<> inline void ossia::supercollider::write(pyrslot* s, bool v)   { SetBool   ( s, v );   }
template<> inline void ossia::supercollider::write(pyrslot* s, pyrobject* o ) { SetObject(s, o); }

template<> inline void ossia::supercollider::write(pyrslot* s, std::string v)
{
    PyrString* str = newPyrString(gMainVMGlobals->gc, v.c_str(), 0, true);
    SetObject( s, str );
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

        ++g->sp; write<pyrobject*>(g->sp, object);
        ++g->sp; write<void*>(g->sp, pointer);
        runInterpreter(g, getsym(sym), 2);

        g->canCallOS = false;
    }

    gLangMutex.unlock();
}

inline void ossia::supercollider::free(vmglobals* g, pyrslot* s)
{
    g->gc->Free(slotRawObject( s ));
    SetNil( s );
}

//----------------------------------------------------------------------------------- CONNECTIONS

tcp_connection::ptr tcp_connection::create(boost::asio::io_context& io_context)
{
    return tcp_connection::ptr( new tcp_connection(io_context) );
}

tcp_connection::tcp_connection( boost::asio::io_context& ctx ) :
    m_socket(ctx) { }

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
    if ( err ) { errpost_return( err ); }
    std::string chunk = m_netbuffer.data();

    m_observer->on_data( netobject::ptr(this),
        bytearray(chunk.begin(), chunk.end()));

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
    if ( err ) { errpost_return( err ); }
    std::cout << "nbytes written: " << std::to_string(nbytes) << std::endl;
}

//----------------------------------------------------------------------------------- SERVER

tcp_server::ptr tcp_server::create(boost::asio::io_context& ctx, uint16_t port)
{
    return tcp_server::ptr(new tcp_server(ctx, port));
}

tcp_server::tcp_server(boost::asio::io_context& ctx, uint16_t port) :
    m_ctx(ctx), m_acceptor(ctx, tcp::endpoint(tcp::v4(), port))
{
    start_accept();
}

tcp_server::~tcp_server()
{
    for ( auto& connection : m_connections )
          connection.reset();
}

tcp_connection::ptr tcp_server::operator[](uint16_t index)
{
    return m_connections[index];
}

tcp_connection::ptr tcp_server::last()
{
    return m_connections.back();
}

void tcp_server::start_accept()
{
    pointer new_connection = tcp_connection::create( m_ctx );

    m_acceptor.async_accept( new_connection->socket(),
        boost::bind(&tcp_server::accept_handler, this, new_connection,
        boost::asio::placeholders::error ) );
}

void tcp_server::accept_handler( tcp_connection::ptr connection,
                                const boost::system::error_code& err )
{
    if ( err ) { errpost_return(err); }

    connection->listen();
    m_connections.push_back( connection );
    start_accept();

    m_observer->on_connection(connection);
}

//----------------------------------------------------------------------------------- CLIENT

tcp_client::ptr tcp_client::create(boost::asio::io_context& ctx)
{
    return tcp_client::ptr(new tcp_client(ctx));
}

tcp_client::tcp_client(boost::asio::io_context& ctx) : m_ctx(ctx)
{
}

tcp_client::~tcp_client()
{
    m_connection.reset();
}

void tcp_client::connect( std::string const& host_addr, uint16_t port )
{
    boost::asio::ip::tcp::endpoint endpoint(
                boost::asio::ip::address::from_string(host_addr), port );

    m_connection = tcp_connection::create( m_ctx );
    auto& socket = m_connection->socket();

    socket.async_connect(endpoint,
        boost::bind(&tcp_client::connected_handler, this,
        m_connection, boost::asio::placeholders::error));
}

void tcp_client::connected_handler(
        tcp_connection::ptr connection, const boost::system::error_code& err )
{
    if ( err ) { errpost_return( err ); }

    m_connection->listen();
    m_observer->on_connection(connection);
}

// ---------------------------------------------------------------------------------- OBSERVERS

template<typename T> sc_observer<T>::sc_observer(pyrslot *slot, T* object)
{
    register_object<T>(slot, object, 0);
    m_object = slotRawObject(slot);
}

template<typename T> void sc_observer<T>::on_connection(netobject::ptr obj)
{
    auto con = dynamic_cast<hwebsocket_connection*>(obj.get());
    sendback_object<hwebsocket_connection>(m_object, con, "pvOnConnection");
}

template<typename T> void sc_observer<T>::on_disconnection(netobject::ptr obj)
{
    auto con = dynamic_cast<hwebsocket_connection*>(obj.get());
    sendback_object<hwebsocket_connection>(m_object, con, "pvOnDisconnection");
}

template<typename T> void sc_observer<T>::on_data(netobject::ptr obj, bytearray data)
{

}

void ws_observer::on_connection(netobject::ptr con)
{
    m_connected_func(con);
}

void ws_observer::on_disconnection(netobject::ptr con)
{
    m_disconnected_func(con);
}

void ws_observer::on_data(netobject::ptr obj, bytearray data)
{
    m_data_func(obj, data);
}

// ---------------------------------------------------------------------------------- WEBSOCKET

hwebsocket_connection::hwebsocket_connection(tcp_connection::ptr con)
    : m_tcp_connection(con)
{
    std::function<void(netobject::ptr, bytearray)> dfunc =
            std::bind( &hwebsocket_connection::on_tcp_data,
                       this, std::placeholders::_1, std::placeholders::_2 );

    auto observer = ws_observer::ptr( new ws_observer );
    observer->set_data_callback( dfunc );
    m_tcp_connection->set_observer( observer );        
}

void hwebsocket_connection::on_tcp_data(netobject::ptr obj, bytearray data)
{
    // parse (binary, text, osc, ...)
    m_observer->on_data(obj, data);
}

void hwebsocket_connection::write_text(std::string text)
{
    // encode and send



}

void hwebsocket_connection::write_binary(bytearray data)
{

}

void hwebsocket_connection::write_raw(bytearray data)
{
    std::string str( data.begin(), data.end() );
    m_tcp_connection->write( str );
}

void hwebsocket_connection::write_osc(std::string address)
{
    // get encoded binary
    // call write_binary
}

// ----

hwebsocket_client::hwebsocket_client(boost::asio::io_context& ctx) :
    m_tcp_client(ctx)
{
    std::function<void(netobject::ptr)> cfunc =
            std::bind( &hwebsocket_client::on_tcp_connected,
                       this, std::placeholders::_1);

    std::function<void(netobject::ptr)> dfunc =
            std::bind( &hwebsocket_client::on_tcp_disconnected,
                       this, std::placeholders::_1 );

    auto observer = ws_observer::ptr( new ws_observer );
    observer->set_connected_callback( cfunc );
    observer->set_disconnected_callback( dfunc );

    m_tcp_client.set_observer( observer );
}

hwebsocket_client::~hwebsocket_client()
{
    // send close
    m_connection.reset();
}

void hwebsocket_client::connect(std::string addr, uint16_t port)
{
    m_tcp_client.connect(addr, port);
}

void hwebsocket_client::disconnect()
{
    m_connection.reset();
}

void hwebsocket_client::on_tcp_connected(netobject::ptr)
{
    // upgrade tcp_connection to websocket
    m_connection = hwebsocket_connection::ptr(
                new hwebsocket_connection(m_tcp_client.connection()) );

    // set data observer until handshake is accepted
    std::function<void(netobject::ptr, bytearray)> dfunc =
            std::bind( &hwebsocket_client::on_tcp_data,
                       this, std::placeholders::_1, std::placeholders::_2 );

    auto observer = ws_observer::ptr( new ws_observer );
    observer->set_data_callback( dfunc );
    m_connection->set_observer( observer );

    // send handshake request
    //m_connection->write_raw();
}

void hwebsocket_client::on_tcp_data(netobject::ptr, bytearray data )
{
    std::string str(data.begin(), data.end());

    if( str.find( "WebSocket-Sec-Accept" ) != std::string::npos )
    {
        // check and validate accept key


        // sc-observer
        m_observer->on_connection(hwebsocket_client::ptr(this));
        m_connection->set_observer(m_observer);
    }

}

void hwebsocket_client::on_tcp_disconnected(netobject::ptr)
{
    m_connection.reset();
    m_observer->on_disconnection(netobject::ptr(this));
}

// ----

hwebsocket_server::hwebsocket_server(boost::asio::io_context& ctx, uint16_t port) :
    m_tcp_server(ctx, port)
{
    std::function<void(netobject::ptr)> cfunc =
            std::bind( &hwebsocket_server::on_new_tcp_connection,
                       this, std::placeholders::_1 );

    std::function<void(netobject::ptr)> dfunc =
            std::bind( &hwebsocket_server::on_tcp_disconnection,
                       this, std::placeholders::_1);

    auto observer = ws_observer::ptr( new ws_observer );
    observer->set_connected_callback( cfunc );
    observer->set_disconnected_callback( dfunc );

    m_tcp_server.set_observer( observer );
}

hwebsocket_server::~hwebsocket_server()
{
    for ( auto& connection : m_connections )
          connection.reset();
}

void hwebsocket_server::on_new_tcp_connection(netobject::ptr object)
{
    // get connection
    tcp_connection::ptr connection(dynamic_cast<tcp_connection*>(object.get()));
    auto observer = ws_observer::ptr( new ws_observer );

    std::function<void(netobject::ptr, bytearray)> dfunc =
            std::bind( &hwebsocket_server::on_tcp_data,
                       this, std::placeholders::_1, std::placeholders::_2 );

    // observe connection's incoming tcp_data
    // look for a websocket handshake pattern
    observer->set_data_callback(dfunc);
    connection->set_observer(observer);
}

void hwebsocket_server::on_tcp_data(netobject::ptr object, bytearray data)
{
    auto str = std::string(data.begin(), data.end());

    if( str.find( "WebSocket-Sec-Key" ) != std::string::npos )
    {
        // parse key and send accept key
        // TODO

        // upgrade tcp_connection to websocket

        tcp_connection::ptr connection(dynamic_cast<tcp_connection*>(object.get()));
        auto ptr = hwebsocket_connection::ptr(new hwebsocket_connection(connection));
        m_connections.push_back(ptr);

        m_observer->on_connection(ptr);
    }
}

void hwebsocket_server::on_tcp_disconnection(netobject::ptr object)
{

}

//----------------------------------------------------------------------------------- PRIMITIVES

int pyr_ws_con_bind(VMGlobals* g, int)
{
    auto con = get_object<hwebsocket_connection>(g->sp, 0);
    auto obs = new sc_observer<hwebsocket_connection>(g->sp, con);

    con->set_observer(netobserver::ptr(obs));

    return errNone;
}

int pyr_ws_con_write_text(VMGlobals* g, int)
{
    auto con = get_object<hwebsocket_connection>(g->sp-1, 0);
    con->write_text(read<std::string>(g->sp));

    return errNone;
}

int pyr_ws_con_write_osc(VMGlobals* g, int)
{
    auto con = get_object<hwebsocket_connection>(g->sp-2, 0);
    //con->write_osc(); // !TODO

    return errNone;
}

int pyr_ws_con_write_binary(VMGlobals* g, int)
{
    auto con = get_object<hwebsocket_connection>(g->sp-1, 0);
    // TODO

    return errNone;
}

int pyr_ws_con_write_raw(VMGlobals* g, int)
{
    auto con = get_object<hwebsocket_connection>(g->sp-1, 0);
    return errNone;
}

// ------------------------------------------

int pyr_ws_client_create(VMGlobals* g, int)
{
    auto client = new hwebsocket_client( ioService );
    auto observer = new sc_observer<hwebsocket_client>(g->sp, client);

    client->set_observer(netobserver::ptr( observer ));

    return errNone;
}

int pyr_ws_client_connect(VMGlobals* g, int)
{
    auto client = get_object<hwebsocket_client>(g->sp-2, 0);
    client->connect(read<std::string>(g->sp-1), read<int>(g->sp));

    return errNone;
}

int pyr_ws_client_disconnect(VMGlobals* g, int)
{
    auto client = get_object<hwebsocket_client>(g->sp, 0);
    return errNone;
}

int pyr_ws_client_free(VMGlobals* g, int)
{
    delete get_object<hwebsocket_client>(g->sp, 0);
    free(g, g->sp);

    return errNone;
}

// ---------------------------------------------------- SERVER

int pyr_ws_server_instantiate_run(VMGlobals* g, int)
{
    auto server = new hwebsocket_server(ioService, read<int>(g->sp));
    auto observer = new sc_observer<hwebsocket_server>(g->sp-1, server);

    return errNone;
}

int pyr_ws_server_free(VMGlobals* g, int)
{
    delete get_object<hwebsocket_server>(g->sp, 0);
    free(g, g->sp);

    return errNone;
}

void ossia::supercollider::initialize()
{
    int base, index = 0;
    base = nextPrimitiveIndex();        

    definePrimitive( base, index++, "_WebSocketConnectionWriteText", pyr_ws_con_write_text, 2, 0);
    definePrimitive( base, index++, "_WebSocketConnectionWriteOSC", pyr_ws_con_write_osc, 2, 0);
    definePrimitive( base, index++, "_WebSocketConnectionWriteBinary", pyr_ws_con_write_binary, 2, 0);
    definePrimitive( base, index++, "_WebSocketConnectionWriteRaw", pyr_ws_con_write_raw, 2, 0);
    definePrimitive( base, index++, "_WebSocketConnectionBind", pyr_ws_con_bind, 1, 0);

    definePrimitive( base, index++, "_WebSocketClientCreate", pyr_ws_client_create, 1, 0);
    definePrimitive( base, index++, "_WebSocketClientConnect", pyr_ws_client_connect, 3, 0);
    definePrimitive( base, index++, "_WebSocketClientDisconnect", pyr_ws_client_disconnect, 1, 0);
    definePrimitive( base, index++, "_WebSocketClientFree", pyr_ws_client_free, 1, 0);

    definePrimitive( base, index++, "_WebSocketServerInstantiateRun", pyr_ws_server_instantiate_run, 2, 0);
    definePrimitive( base, index++, "_WebSocketServerInstantiateRun", pyr_ws_server_free, 1, 0);
}
