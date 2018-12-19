#pragma once

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

using boost::asio::ip::tcp;

#define TCP_BUFSIZE 32768

namespace ossia
{
namespace supercollider
{
void initialize();
}
}

class tcp_connection : public boost::enable_shared_from_this<tcp_connection>
{
    public:

    using  pointer = boost::shared_ptr<tcp_connection>;
    static pointer create(boost::asio::io_context& ioctx);
    tcp_connection( boost::asio::io_context& ctx );

    tcp::socket& socket() { return m_socket; }
    void write(const std::string& str);
    void listen();

    private:
    void read_handler(const boost::system::error_code& err, size_t nbytes);
    void write_handler(const boost::system::error_code& err, size_t nbytes);
    tcp::socket m_socket;
    std::array<char, 128> m_netbuffer;
};

class tcp_client : public boost::enable_shared_from_this<tcp_client>
{
    public:

    tcp_client(boost::asio::io_context& ctx );
    ~tcp_client();

    void connect(const std::string& host_addr, uint16_t host_port );
    void write(const std::string& message);

    private:
    void connected_handler(tcp_connection::pointer con, const boost::system::error_code& err );
    tcp_connection::pointer m_connection;
};

class tcp_server
{
    public:
    tcp_server  ( boost::asio::io_context& ctx, uint16_t port );
    ~tcp_server ( );

    private:
    void start_accept ( );
    void accept_handler(tcp_connection::pointer connection, const boost::system::error_code& err);
    tcp::acceptor m_acceptor;
    std::vector<tcp_connection::pointer> m_connections;

};
