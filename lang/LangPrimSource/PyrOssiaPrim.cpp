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

template<> inline bool sclang::read( pyrslot* s)
{
    return IsTrue( s );
}

template<> inline float sclang::read(pyrslot* s)
{
    float f; slotFloatVal(s, &f); return f;
}

template<> inline int sclang::read(pyrslot* s)
{
    int i; slotIntVal(s, &i); return i;
}

template<> inline std::string sclang::read(pyrslot* s)
{
    char v[ STRMAXLE ]; slotStrVal(s, v, STRMAXLE);
    return static_cast<std::string>(v);
}

template<> inline void sclang::write(pyrslot* s, int v)          { SetInt    ( s, v );   }
template<> inline void sclang::write(pyrslot* s, float v)        { SetFloat  ( s, v );   }
template<> inline void sclang::write(pyrslot* s, double v)       { SetFloat  ( s, v );   }
template<> inline void sclang::write(pyrslot* s, void* v)        { SetPtr    ( s, v );   }
template<> inline void sclang::write(pyrslot* s, bool v)         { SetBool   ( s, v );   }
template<> inline void sclang::write(pyrslot* s, pyrobject* o )  { SetObject ( s, o );   }
template<typename T> inline void sclang::write(pyrslot* s, T o)  { SetPtr    ( s, o );   }

template<> inline void sclang::write(pyrslot* s, std::string v)
{
    PyrString* str = newPyrString( gMainVMGlobals->gc, v.c_str(), 0, true );
    SetObject( s, str );
}

template<typename T> inline void sclang::write(pyrslot* s, T object, uint16_t index )
{
    pyrslot* ivar = slotRawObject(s)->slots+index;
    SetPtr(  ivar, object );
}

template<typename T> inline T sclang::read(pyrslot* s, uint16_t index)
{
    return static_cast<T>(slotRawPtr(&slotRawObject(s)->slots[index]));
}

template<typename T> void sclang::return_data(pyrobject* object, T data, const char* sym)
{
    gLangMutex.lock();

    if ( compiledOK )
    {
        auto g = gMainVMGlobals;
        g->canCallOS = true;

        ++g->sp; sclang::write<pyrobject*>(g->sp, object);
        ++g->sp; sclang::write<T>(g->sp, data);
        runInterpreter(g, getsym(sym), 2);

        g->canCallOS = false;
    }

    gLangMutex.unlock();
}

template<typename T>
void sclang::return_data(pyrobject* object, std::vector<T> data, const char* sym)
{
    // TODO
    gLangMutex.lock();

    if ( compiledOK )
    {
        auto g = gMainVMGlobals;
        g->canCallOS = true;

        ++g->sp; sclang::write<pyrobject*>(g->sp, object);

        for ( const auto& d : data )
        {
            ++g->sp;
            sclang::write<T>(g->sp, d);
        }

        runInterpreter(g, getsym(sym), data.size()+1);
        g->canCallOS = false;
    }

    gLangMutex.unlock();
}

template<typename T> inline void sclang::free(pyrslot* s, T data)
{
    gMainVMGlobals->gc->Free(slotRawObject( s ));
    SetNil( s );
    delete data;
}

//----------------------------------------------------------------------------------- CONNECTIONS
using namespace network::tcp;

connection::ptr connection::create(boost::asio::io_context& ctx)
{
    return tcp::connection::ptr( new tcp::connection(ctx) );
}

connection::connection( boost::asio::io_context& ctx ) :
    m_socket(ctx) { }

inline std::string connection::remote_address() const
{
    return m_socket.remote_endpoint().address().to_v4().to_string();
}

inline uint16_t connection::remote_port() const
{
    return m_socket.remote_endpoint().port();
}

void connection::listen()
{    
    boost::asio::async_read(m_socket, boost::asio::buffer(m_netbuffer),
        boost::asio::transfer_at_least(1),
        boost::bind(&connection::read_handler, shared_from_this(),
        boost::asio::placeholders::error,
        boost::asio::placeholders::bytes_transferred));
}

void connection::read_handler(const boost::system::error_code& err, size_t nbytes)
{        
    if ( err ) { errpost_return( err ); }
    std::string chunk = m_netbuffer.data();

    m_observer->on_data(
        network::object::ptr(this), data_t::raw,
        bytearray(chunk.begin(), chunk.end()));

    listen();
}

void connection::write(const std::string& data)
{
    auto buf = boost::asio::buffer( data );

    boost::asio::async_write( m_socket, buf,
        boost::bind( &connection::write_handler, shared_from_this(),
        boost::asio::placeholders::error,
        boost::asio::placeholders::bytes_transferred ) );
}

void connection::write_handler(const boost::system::error_code& err, size_t nbytes)
{
    if ( err ) { errpost_return( err ); }
    std::cout << "nbytes written: " << std::to_string(nbytes) << std::endl;
}

//----------------------------------------------------------------------------------- SERVER
server::ptr server::create(boost::asio::io_context& ctx, uint16_t port)
{
    return server::ptr(new server(ctx, port));
}

server::server(boost::asio::io_context& ctx, uint16_t port) :
    m_ctx(ctx), m_acceptor(ctx, boost::asio::ip::tcp::endpoint(
                                boost::asio::ip::tcp::v4(), port))
{
    start_accept();
}

server::~server()
{
    for ( auto& connection : m_connections )
          connection.reset();
}

connection::ptr server::operator[](uint16_t index)
{
    return m_connections[index];
}

connection::ptr server::last()
{
    return m_connections.back();
}

void server::start_accept()
{
    auto new_connection = connection::create( m_ctx );

    m_acceptor.async_accept( new_connection->socket(),
        boost::bind(&server::accept_handler, this, new_connection,
        boost::asio::placeholders::error ) );
}

void server::accept_handler( connection::ptr connection,
                                const boost::system::error_code& err )
{
    if ( err ) { errpost_return(err); }

    connection->listen();
    m_connections.push_back( connection );
    start_accept();

    m_observer->on_connection(connection);
}

//----------------------------------------------------------------------------------- CLIENT

client::ptr client::create(boost::asio::io_context& ctx)
{
    return client::ptr(new client(ctx));
}

client::client(boost::asio::io_context& ctx) : m_ctx(ctx)
{
}

client::~client()
{
    m_connection.reset();
}

void client::connect( std::string const& host_addr, uint16_t port )
{
    boost::asio::ip::tcp::endpoint endpoint(
                boost::asio::ip::address::from_string(host_addr), port );

    m_connection = connection::create( m_ctx );
    auto& socket = m_connection->socket();

    socket.async_connect(endpoint,
        boost::bind(&client::connected_handler, this,
        m_connection, boost::asio::placeholders::error));
}

void client::connected_handler(
        connection::ptr connection, const boost::system::error_code& err )
{
    if ( err ) { errpost_return( err ); }

    m_connection->listen();
    m_observer->on_connection(connection);
}

// ---------------------------------------------------------------------------------- OBSERVERS
using namespace network;

void generic_observer::on_connection(object::ptr p)
{
    on_connection_f(p);
}

void generic_observer::on_disconnection(object::ptr p)
{
    on_disconnection_f(p);
}

void generic_observer::on_data(object::ptr p, data_t t, bytearray d)
{
    on_data_f(p, t, d);
}

template<typename T> boost::shared_ptr<sc_observer<T>> create(pyrslot* s, T* o)
{
    return sc_observer<T>::ptr(new sc_observer<T>(s, o));
}

template<typename T> sc_observer<T>::sc_observer(pyrslot *slot, T* obj)
{
    sclang::write<T*>(slot, obj, 0);
    m_pyrobject = slotRawObject(slot);
}

template<typename T> void sc_observer<T>::on_connection(object::ptr obj)
{
    auto con = dynamic_cast<T*>(obj.get());
    sclang::return_data<T*>(m_pyrobject, con, "pvOnConnection");
}

template<typename T> void sc_observer<T>::on_disconnection(object::ptr obj)
{
    auto con = dynamic_cast<T*>(obj.get());
    sclang::return_data<T*>(m_pyrobject, con, "pvOnDisconnection");
}

template<typename T>
void sc_observer<T>::on_data(object::ptr obj, data_t type, bytearray data)
{
    auto con = dynamic_cast<T*>(obj.get());

    if ( type == data_t::http )
         on_http_data(boost::shared_ptr<T>(con), std::string(data.begin(), data.end()));

    else if ( type == data_t::ws_text )
         on_text_data(boost::shared_ptr<T>(con), std::string(data.begin(), data.end()));
}

template<typename T> void sc_observer<T>::on_osc_data(boost::shared_ptr<T>, std::string )
{
    // send back a string address and an array of data

}

template<typename T> void sc_observer<T>::on_http_data(boost::shared_ptr<T>, std::string data )
{
    gLangMutex.lock();

    auto g = gMainVMGlobals;
    ++g->sp; sclang::write<pyrobject*>(g->sp, m_pyrobject);
    ++g->sp; sclang::write<std::string>(g->sp, data);
    runInterpreter(g, getsym("pvOnHTTPMessageReceived"), 2);

    gLangMutex.unlock();
}

template<typename T> void sc_observer<T>::on_text_data(boost::shared_ptr<T>, std::string data )
{
    gLangMutex.lock();

    auto g = gMainVMGlobals;
    ++g->sp; sclang::write<pyrobject*>(g->sp, m_pyrobject);
    ++g->sp; sclang::write<std::string>(g->sp, data);
    runInterpreter(g, getsym("pvOnTextMessageReceived"), 2);

    gLangMutex.unlock();
}

template<typename T> void sc_observer<T>::on_binary_data(boost::shared_ptr<T>, bytearray )
{
    // sendback an int8array ?
    // TODO
}

// ---------------------------------------------------------------------------------- WEBSOCKET
using namespace network::websocket;

std::string websocket::generate_sec_key()
{
    return std::string();
}

std::string websocket::generate_accept_key(std::string const& sec_key)
{
    return std::string();
}

websocket::message websocket::message::decode(bytearray data)
{

}

websocket::message websocket::message::encode(bytearray data)
{

}

websocket::message websocket::message::encode(std::string data)
{

}

websocket::message::message(bytearray data)
{

}

template<typename T> T websocket::message::read() const
{

}

//------------

websocket::connection::connection(tcp::connection::ptr con)
    : m_tcp_connection(con)
{
    std::function<void(object::ptr, data_t, bytearray)> dfunc =
            std::bind( &websocket::connection::on_tcp_data, this,
                       std::placeholders::_1, std::placeholders::_2, std::placeholders::_3 );

    auto observer = generic_observer::ptr( new generic_observer );
    observer->on_data_f = dfunc;

    m_tcp_connection->set_observer( observer );        
}

void websocket::connection::on_tcp_data(
    object::ptr obj, data_t t, bytearray data )
{
    m_observer->on_data(obj, t, data);
}

void websocket::connection::write_text(std::string text)
{
    // encode and send
    auto encoded = websocket::message::encode(text);
    m_tcp_connection->write(encoded.read<std::string>());
}

void websocket::connection::write_binary(bytearray data)
{

}

void websocket::connection::write_raw(bytearray data)
{
    std::string str( data.begin(), data.end() );
    m_tcp_connection->write( str );
}

void websocket::connection::write_osc(std::string address)
{
    // get encoded binary
    // call write_binary
}

// ----

websocket::client::client(boost::asio::io_context& ctx) :
    m_tcp_client(ctx)
{
    std::function<void(object::ptr)> cfunc =
            std::bind( &websocket::client::on_tcp_connected,
                       this, std::placeholders::_1);

    std::function<void(object::ptr)> dfunc =
            std::bind( &websocket::client::on_tcp_disconnected,
                       this, std::placeholders::_1 );

    auto observer = generic_observer::ptr( new generic_observer );
    observer->on_connection_f = cfunc;
    observer->on_disconnection_f = dfunc;
    m_tcp_client.set_observer( observer );
}

websocket::client::~client()
{
    // send close
    m_connection.reset();
}

void websocket::client::connect(std::string addr, uint16_t port)
{
    m_tcp_client.connect(addr, port);
}

void websocket::client::disconnect()
{
    m_connection.reset();
}

void websocket::client::on_tcp_connected(object::ptr)
{
    // upgrade tcp_connection to websocket
    m_connection = websocket::connection::ptr(
        new websocket::connection(m_tcp_client.connection()) );

    // set data observer until handshake is accepted
    std::function<void(object::ptr, data_t, bytearray)> dfunc =
            std::bind( &websocket::client::on_tcp_data, this,
                       std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

    auto observer = generic_observer::ptr( new generic_observer );
    observer->on_data_f = dfunc;
    m_connection->set_observer( observer );

    // send handshake request
    std::string handshake( "GET / HTTP/1.1\r\n" );
    handshake.append( "Connection: Upgrade\r\n" );
    handshake.append( "Sec-WebSocket-Key: " );
    handshake.append( websocket::generate_sec_key().append( "\r\n" ));
    handshake.append( "Sec-WebSocket-Version: 13\r\n" );
    handshake.append( "Upgrade: websocket\r\n" );
    handshake.append( "User-Agent: SuperCollider\r\n" );

    m_connection->write_raw(bytearray(handshake.begin(), handshake.end()));
}

void websocket::client::on_tcp_data(object::ptr, data_t, bytearray data )
{
    std::string str(data.begin(), data.end());

    if( str.find( "WebSocket-Sec-Accept" ) != std::string::npos )
    {
        // check and validate accept key


        // sc-observer
        m_observer->on_connection(websocket::client::ptr(this));
        m_connection->set_observer(m_observer);
    }

}

void websocket::client::on_tcp_disconnected(object::ptr)
{
    m_connection.reset();
    m_observer->on_disconnection(object::ptr(this));
}

// ----

websocket::server::server(boost::asio::io_context& ctx, uint16_t port) :
    m_tcp_server(ctx, port)
{
    std::function<void(object::ptr)> cfunc =
            std::bind( &websocket::server::on_new_tcp_connection,
                       this, std::placeholders::_1 );

    std::function<void(object::ptr)> dfunc =
            std::bind( &websocket::server::on_tcp_disconnection,
                       this, std::placeholders::_1);

    auto observer = generic_observer::ptr( new generic_observer );
    observer->on_connection_f = cfunc;
    observer->on_disconnection_f = dfunc;

    m_tcp_server.set_observer( observer );
}

websocket::server::~server()
{
    for ( auto& connection : m_connections )
          connection.reset();
}

void websocket::server::on_new_tcp_connection(object::ptr object)
{
    // get connection
    tcp::connection::ptr connection(dynamic_cast<tcp::connection*>(object.get()));
    auto observer = generic_observer::ptr( new generic_observer );

    std::function<void(object::ptr, data_t, bytearray)> dfunc =
            std::bind( &websocket::server::on_tcp_data, this,
                       std::placeholders::_1, std::placeholders::_2 , std::placeholders::_3);

    // observe connection's incoming tcp_data
    // look for a websocket handshake pattern
    observer->on_data_f = dfunc;
    connection->set_observer( observer );
}

void websocket::server::on_tcp_data(object::ptr object, data_t, bytearray data)
{
    auto str = std::string(data.begin(), data.end());

    if( str.find( "WebSocket-Sec-Key" ) != std::string::npos )
    {
        // parse key and send accept key
        // TODO

        // upgrade tcp_connection to websocket
        tcp::connection::ptr connection(dynamic_cast<tcp::connection*>(object.get()));
        auto ptr = websocket::connection::ptr(
                   new websocket::connection(connection));

        m_connections.push_back(ptr);
        m_observer->on_connection(ptr);
    }
}

void websocket::server::on_tcp_disconnection(object::ptr object)
{

}

//----------------------------------------------------------------------------------- PRIMITIVES

int pyr_ws_con_bind(VMGlobals* g, int)
{
    auto con = sclang::read<websocket::connection*>(g->sp, 0);
    auto obs = new sc_observer<websocket::connection>(g->sp, con);
    con->set_observer(sc_observer<websocket::connection>::ptr(obs));

    return errNone;
}

int pyr_ws_con_write_text(VMGlobals* g, int)
{
    auto con = sclang::read<websocket::connection*>(g->sp-1, 0);
    con->write_text(sclang::read<std::string>(g->sp));

    return errNone;
}

int pyr_ws_con_write_osc(VMGlobals* g, int)
{
    auto con = sclang::read<websocket::connection*>(g->sp-2, 0);
    //con->write_osc(); // !TODO

    return errNone;
}

int pyr_ws_con_write_binary(VMGlobals* g, int)
{
    auto con = sclang::read<websocket::connection*>(g->sp-1, 0);
    // TODO

    return errNone;
}

int pyr_ws_con_write_raw(VMGlobals* g, int)
{
    auto con = sclang::read<websocket::connection*>(g->sp-1, 0);
    // TODO

    return errNone;
}

// ------------------------------------------

int pyr_ws_client_create(VMGlobals* g, int)
{
    auto client = new websocket::client( ioService );
    auto observer = new sc_observer<websocket::client>(g->sp, client);
    client->set_observer(boost::shared_ptr<sc_observer<websocket::client>>(observer));

    return errNone;
}

int pyr_ws_client_connect(VMGlobals* g, int)
{
    auto client = sclang::read<websocket::client*>(g->sp-2, 0);

    client->connect( sclang::read<std::string>(g->sp-1),
                     sclang::read<int>(g->sp));

    return errNone;
}

int pyr_ws_client_disconnect(VMGlobals* g, int)
{
    auto client = sclang::read<websocket::client*>(g->sp, 0);
    client->disconnect();

    return errNone;
}

int pyr_ws_client_free(VMGlobals* g, int)
{
    auto client = sclang::read<websocket::client*>(g->sp, 0);
    sclang::free(g->sp, client);

    return errNone;
}

// ---------------------------------------------------- SERVER

int pyr_ws_server_instantiate_run(VMGlobals* g, int)
{
    auto server = new websocket::server( ioService, sclang::read<int>(g->sp) );
    auto observer = new sc_observer<websocket::server>(g->sp-1, server);
    server->set_observer(boost::shared_ptr<sc_observer<websocket::server>>(observer));

    return errNone;
}

int pyr_ws_server_free(VMGlobals* g, int)
{
    auto server = sclang::read<websocket::server*>(g->sp, 0);
    sclang::free(g->sp, server);

    return errNone;
}

void network::initialize()
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
    definePrimitive( base, index++, "_WebSocketServerFree", pyr_ws_server_free, 1, 0);
}
