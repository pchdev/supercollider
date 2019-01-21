#pragma once

#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/enable_shared_from_this.hpp>

#if defined(__APPLE__) && !defined(SC_IPHONE)
#include <CoreServices/CoreServices.h>

#elif HAVE_AVAHI
#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-common/alternative.h>
#include <avahi-common/error.h>
#include <avahi-common/malloc.h>
#include <avahi-common/thread-watch.h>
#include <avahi-common/timeval.h>
#endif

#include "PyrSymbolTable.h"
#include "PyrSched.h"
#include "PyrPrimitive.h"
#include "PyrKernel.h"
#include "PyrSymbol.h"
#include "PyrInterpreter.h"
#include "GC.h"
#include "SC_LanguageClient.h"
// ----------------------------------------------------------------------------
using boost::asio::ip::tcp;
// ----------------------------------------------------------------------------
using pyrslot   = PyrSlot;
using pyrobject = PyrObject;
using vmglobals = VMGlobals;
// ----------------------------------------------------------------------------
#define TCP_BUFSIZE 32768
#define WS_GENERIC_T template<typename T>
#define WS_OPTIMIZE
#define WS_REFACTOR
#define WS_UNIMPLEMENTED
// ----------------------------------------------------------------------------
// SCLANG-GENERIC-UTILITIES
namespace sclang {
//-----------------------------------------------------------------------------
WS_GENERIC_T       // calls 'sym' sc-method, passing single data as argument
void return_data   ( pyrobject* object, T data, const char* sym );
//-----------------------------------------------------------------------------
WS_GENERIC_T       // calls 'sym' sc-method, passing mutiple data as arguments
void return_data   ( pyrobject* object, std::vector<T>, const char* sym);
//-----------------------------------------------------------------------------
WS_GENERIC_T      // pushes object 'T' to slot 's'
void write         ( pyrslot* s, T object);
//-----------------------------------------------------------------------------
WS_GENERIC_T      // pushes object 'T' to  object's instvar 'index'
void write         ( pyrslot* s, T object, uint16_t index );
//-----------------------------------------------------------------------------
WS_GENERIC_T      // reads object 'T' from object's instvar 'index'
T read             ( pyrslot* s, uint16_t index );
//-----------------------------------------------------------------------------
WS_GENERIC_T      // reads object 'T' from slot 's'
T read             ( pyrslot* s );
//-----------------------------------------------------------------------------
WS_GENERIC_T      // frees object from slot and heap
void free          ( pyrslot* s, T object );
}
// ----------------------------------------------------------------------------
// NETWORK-OBSERVERS
namespace network {
// ----------------------------------------------------------------------------
using bytearray = std::vector<uint8_t>;

/// initializes sc-websocket primitives --------------------------------------
void initialize();
//----------------------------------------------------------------------------
enum class data_t { raw = 0, http = 1, text = 2, binary = 3, osc = 4 };
//----------------------------------------------------------------------------
class observer;
// ----------------------------------------------------------------------------
class object
// ----------------------------------------------------------------------------
{
    public:
    using ptr = boost::shared_ptr<object>;
    virtual void set_observer( boost::shared_ptr<observer> obs )  {
        m_observer = obs;
    }

    protected:
    boost::shared_ptr<observer> m_observer;
};
// ----------------------------------------------------------------------------
class observer
// ----------------------------------------------------------------------------
{
    public:
    using ptr = boost::shared_ptr<observer>;

    virtual void on_connection      ( object::ptr) = 0;
    virtual void on_disconnection   ( object::ptr) = 0;
    virtual void on_data( object::ptr, data_t, bytearray) = 0;
};
// ----------------------------------------------------------------------------
class generic_observer final : public network::observer
// ----------------------------------------------------------------------------
{
    public:
    using ptr = boost::shared_ptr<generic_observer>;

    generic_observer() { }
    virtual void on_connection      ( object::ptr ) override;
    virtual void on_disconnection   ( object::ptr ) override;
    virtual void on_data( object::ptr, data_t, bytearray data ) override;

    std::function<void( network::object::ptr )> on_connection_f;
    std::function<void( network::object::ptr )> on_disconnection_f;
    std::function<void( network::object::ptr, data_t, bytearray)> on_data_f;
};

// ----------------------------------------------------------------------------
// BOOST-TCP
namespace tcp {
// ----------------------------------------------------------------------------
class connection final : public network::object,
                         public boost::enable_shared_from_this<connection>
// ----------------------------------------------------------------------------
{
    public:
    using ptr = boost::shared_ptr<connection>;

    static ptr create(boost::asio::io_context& ioctx);
    connection( boost::asio::io_context& ctx );

    boost::asio::ip::tcp::socket& socket() { return m_socket; }
    void write(const std::string& str);
    void listen();
    std::string remote_address() const;
    uint16_t remote_port() const;

    private:
    void read_handler   ( const boost::system::error_code& err, size_t nbytes);
    void write_handler  ( const boost::system::error_code& err, size_t nbytes);

    boost::asio::ip::tcp::socket m_socket;
    std::array<char, TCP_BUFSIZE> m_netbuffer;
};

// ----------------------------------------------------------------------------
class client final : public object,
                     public boost::enable_shared_from_this<client>
// ----------------------------------------------------------------------------
{
    public:
    using ptr = boost::shared_ptr<client>;

    static ptr create(boost::asio::io_context& ctx);
    client(boost::asio::io_context& ctx);

    void connect(const std::string& host_addr, uint16_t host_port );
    tcp::connection::ptr connection() { return m_connection; }

    ~client();

    private:
    void connected_handler(connection::ptr con,
                           const boost::system::error_code& err );

    connection::ptr m_connection;
    boost::asio::io_context& m_ctx;
};

// ----------------------------------------------------------------------------
class server final : public object
// ----------------------------------------------------------------------------
{
    public:
    using ptr = boost::shared_ptr<server>;

    static server::ptr create(boost::asio::io_context &ctx, uint16_t port);
    server(boost::asio::io_context& ctx, uint16_t port);

    connection::ptr operator[](uint16_t index);
    connection::ptr last();

    ~server();

    private:
    void start_accept();

    void accept_handler(connection::ptr connection,
                        const boost::system::error_code& err);

    boost::asio::io_context& m_ctx;
    boost::asio::ip::tcp::acceptor m_acceptor;
    std::vector<connection::ptr> m_connections;
};

}
// ----------------------------------------------------------------------------
// HTTP
namespace http {
// ----------------------------------------------------------------------------

// HTTP managed by SC ?
// what about threads ?
}

// ----------------------------------------------------------------------------
// WEBSOCKET
namespace websocket {
// ----------------------------------------------------------------------------
static std::string generate_sec_key();
static std::string generate_accept_key(std::string &sec_key);
// ----------------------------------------------------------------------------
class connection final : public network::object
// ----------------------------------------------------------------------------
{
    public:
    enum owner
    {
        client = 0,
        server = 1
    };

    using ptr = boost::shared_ptr<connection>;
    connection( tcp::connection::ptr con, owner own);

    owner get_owner() const { return m_owner; }

    void write_text     ( std::string text );
    void write_binary   ( bytearray data );
    void write_raw      ( bytearray data );
    void write_osc      ( std::string address);

    void on_tcp_data    ( object::ptr, data_t t, bytearray data );

    private:
    bool m_upgraded = false;
    tcp::connection::ptr m_tcp_connection;
    enum owner m_owner;
};
// ----------------------------------------------------------------------------
class message
// ----------------------------------------------------------------------------
{
    public:

    static message decode(bytearray data, connection::owner own);
    static message encode(bytearray data, connection::owner own);
    static message encode(std::string data, connection::owner own);

    WS_GENERIC_T T read() const;
    message(bytearray data);

    private:
    bytearray _data;
};
// ----------------------------------------------------------------------------
class client final : public network::object
// ----------------------------------------------------------------------------
{
    public:
    client(boost::asio::io_context& ctx);

    websocket::connection::ptr connection();
    void connect(std::string addr, uint16_t port);
    void disconnect();

    void on_tcp_data            ( object::ptr, data_t, bytearray data );
    void on_tcp_connected       ( object::ptr );
    void on_tcp_disconnected    ( object::ptr );

    ~client();

    private:
    tcp::client m_tcp_client;
    websocket::connection::ptr m_connection;
};
// ----------------------------------------------------------------------------
class server final : public object
// ----------------------------------------------------------------------------
{
    public:
    server(boost::asio::io_context& ctx, uint16_t port);
    websocket::connection::ptr operator[](uint16_t index);

    void on_tcp_data            ( object::ptr, data_t, bytearray );
    void on_new_tcp_connection  ( object::ptr );
    void on_tcp_disconnection   ( object::ptr );

    ~server();

    private:
    std::vector<websocket::connection::ptr> m_connections;
    tcp::server m_tcp_server;
};
}
// ----------------------------------------------------------------------------
WS_GENERIC_T
class sc_observer final : public network::observer
// ----------------------------------------------------------------------------
{
    public:
    using ptr = boost::shared_ptr<sc_observer<T>>;
    sc_observer( pyrslot* slot, T* obj);

    static boost::shared_ptr<sc_observer<T>>
    create(pyrslot* s, T* obj);

    virtual void on_connection      ( object::ptr ) override;
    virtual void on_disconnection   ( object::ptr ) override;
    virtual void on_data            ( object::ptr, data_t, bytearray ) override;

    private:
    void on_binary_data  ( boost::shared_ptr<T>, bytearray );
    void on_text_data    ( boost::shared_ptr<T>, std::string );
    void on_http_data    ( boost::shared_ptr<T>, std::string );
    void on_osc_data     ( boost::shared_ptr<T>, std::string );

    pyrobject* m_pyrobject;
};

}
