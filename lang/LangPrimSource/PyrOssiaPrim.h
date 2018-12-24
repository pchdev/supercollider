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

using boost::asio::ip::tcp;
using pyrslot = PyrSlot;
using pyrobject = PyrObject;
using vmglobals = VMGlobals;

#define TCP_BUFSIZE 32768

namespace ossia
{
namespace supercollider
{

void initialize();

using bytearray = std::vector<uint8_t>;

class netobserver;

class netobject
{
    public:
    using ptr = boost::shared_ptr<netobject>;
    virtual void set_observer( boost::shared_ptr<netobserver> observer )  {
        m_observer = observer;
    }

    protected:
    boost::shared_ptr<netobserver> m_observer;
};

class netobserver
{
    public:
    using ptr = boost::shared_ptr<netobserver>;

    virtual void on_connection(netobject::ptr) = 0;
    virtual void on_disconnection(netobject::ptr) = 0;
    virtual void on_data(netobject::ptr, bytearray) = 0;
};

class ws_observer : public netobserver
{
    public:
    using ptr = boost::shared_ptr<ws_observer>;

    ws_observer() { }
    virtual void on_connection(netobject::ptr) override;
    virtual void on_disconnection(netobject::ptr) override;
    virtual void on_data(netobject::ptr, bytearray data) override;

    void set_connected_callback(std::function<void(netobject::ptr)> func) {
        m_connected_func = func;
    }
    void set_disconnected_callback(std::function<void(netobject::ptr)> func) {
        m_disconnected_func = func;
    }
    void set_data_callback(std::function<void(netobject::ptr, bytearray)> func) {
        m_data_func = func;
    }

    private:
    std::function<void(netobject::ptr)> m_connected_func;
    std::function<void(netobject::ptr)> m_disconnected_func;
    std::function<void(netobject::ptr, bytearray)> m_data_func;
};

template<typename T> class sc_observer : public netobserver
{
    public:
    sc_observer( pyrslot* slot, T* object );

    virtual void on_connection(netobject::ptr) override;
    virtual void on_disconnection(netobject::ptr) override;
    virtual void on_data(netobject::ptr, bytearray) override;

    void on_binary_data(netobject::ptr, bytearray);
    void on_text_data(netobject::ptr, std::string);
    void on_http_data(netobject::ptr, std::string);
    void on_osc_data(netobject::ptr, std::string);

    private:
    pyrobject* m_object;
    std::string m_csym;
    std::string m_dsym;
    std::string m_datasym;
};

class tcp_connection :  public netobject,
                        public boost::enable_shared_from_this<tcp_connection>
{
    public:

    using  ptr = boost::shared_ptr<tcp_connection>;
    static ptr create(boost::asio::io_context& ioctx);
    tcp_connection( boost::asio::io_context& ctx );

    tcp::socket& socket() { return m_socket; }

    void write(const std::string& str);
    void listen();

    std::string remote_address() const;
    uint16_t remote_port() const;

    private:
    void read_handler(const boost::system::error_code& err, size_t nbytes);
    void write_handler(const boost::system::error_code& err, size_t nbytes);
    tcp::socket m_socket;
    std::array<char, TCP_BUFSIZE> m_netbuffer;    
};

class tcp_client : public netobject, public boost::enable_shared_from_this<tcp_client>
{
    public:

    using ptr = boost::shared_ptr<tcp_client>;
    static ptr create(boost::asio::io_context& ctx);
    tcp_client(boost::asio::io_context& ctx);

    void connect(const std::string& host_addr, uint16_t host_port );
    tcp_connection::ptr connection() { return m_connection; }

    ~tcp_client();

    private:
    void connected_handler(tcp_connection::ptr con, const boost::system::error_code& err );
    tcp_connection::ptr m_connection;
    boost::asio::io_context& m_ctx;
};

class tcp_server : public netobject
{
    public:
    using ptr = boost::shared_ptr<tcp_server>;
    static tcp_server::ptr create(boost::asio::io_context &ctx, uint16_t port);
    tcp_server(boost::asio::io_context& ctx, uint16_t port);

    tcp_connection::ptr operator[](uint16_t index);
    tcp_connection::ptr last();

    ~tcp_server();

    private:
    void start_accept();
    void accept_handler(tcp_connection::ptr connection, const boost::system::error_code& err);

    boost::asio::io_context& m_ctx;
    tcp::acceptor m_acceptor;
    std::vector<tcp_connection::ptr> m_connections;
};

class hwebsocket_connection : public netobject
{
    public:
    using ptr = boost::shared_ptr<hwebsocket_connection>;
    hwebsocket_connection( tcp_connection::ptr con );

    void write_text     ( std::string text );
    void write_binary   ( bytearray data );
    void write_raw      ( bytearray data );
    void write_osc      ( std::string address);

    void on_tcp_data    ( netobject::ptr, bytearray data );

    private:
    tcp_connection::ptr m_tcp_connection;
};

class hwebsocket_client : public netobject
{
    public:
    hwebsocket_client(boost::asio::io_context& ctx);
    ~hwebsocket_client();

    hwebsocket_connection::ptr connection();
    void connect(std::string addr, uint16_t port);
    void disconnect();

    void on_tcp_data(netobject::ptr, bytearray data);
    void on_tcp_connected(netobject::ptr);
    void on_tcp_disconnected(netobject::ptr);

    private:
    tcp_client m_tcp_client;
    hwebsocket_connection::ptr m_connection;
};

class hwebsocket_server : public netobject
{
    public:
    hwebsocket_server(boost::asio::io_context& ctx, uint16_t port);
    hwebsocket_connection::ptr operator[](uint16_t index);

    void on_tcp_data(netobject::ptr, bytearray);
    void on_new_tcp_connection(netobject::ptr);
    void on_tcp_disconnection(netobject::ptr);

    ~hwebsocket_server();

    private:
    std::vector<hwebsocket_connection::ptr> m_connections;
    tcp_server m_tcp_server;
};

template<typename T> void sendback_object   ( pyrobject* object, T* pointer, const char* sym );
template<typename T> void register_object   ( pyrslot* s, T* object, uint16_t v_index );
template<typename T> T* get_object          ( pyrslot* s, uint16_t v_index );
template<typename T> T read                 ( pyrslot* s );
template<typename T> void write             ( pyrslot* s, T );

void free( vmglobals *g, pyrslot* s );

}
}
