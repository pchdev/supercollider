#include "PyrOssiaPrim.h"

#include "PyrSymbolTable.h"
#include "PyrSched.h"
#include "PyrPrimitive.h"
#include "PyrKernel.h"
#include "PyrSymbol.h"
#include "PyrInterpreter.h"
#include "GC.h"
#include "SC_LanguageClient.h"

#include <boost/bind.hpp>
#include <iostream>

using boost::asio::ip::tcp;
using pointer = boost::shared_ptr<tcp_connection>;
boost::asio::io_context g_ioctx;

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
        boost::asio::transfer_at_least(20),
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

tcp_server::tcp_server( boost::asio::io_context& ctx, uint16_t port ) :
    m_acceptor(ctx, tcp::endpoint(tcp::v4(), 13))
{
    start_accept();
}

void tcp_server::start_accept()
{
    pointer new_connection = tcp_connection::create( m_acceptor.get_executor().context() );

    m_acceptor.async_accept( new_connection->socket(),
        boost::bind(&tcp_server::accept_handler, this, new_connection,
        boost::asio::placeholders::error ) );
}

void tcp_server::accept_handler( tcp_connection::pointer connection,
                                const boost::system::error_code& err )
{
    if ( !err ) m_connections.push_back( connection );
    start_accept();
}

//----------------------------------------------------------------------------------- CLIENT

tcp_client::tcp_client(boost::asio::io_context& ctx)
{

}

void tcp_client::connect( const std::string& host_addr, uint16_t port )
{
    boost::asio::ip::tcp::endpoint endpoint(
                boost::asio::ip::address::from_string(host_addr), port );

    auto connection = tcp_connection::create(g_ioctx);
    auto& socket = connection->socket();

    socket.async_connect(endpoint,
        boost::bind(&tcp_client::connected_handler, this,
        connection, boost::asio::placeholders::error));
}

void tcp_client::connected_handler(tcp_connection::pointer con, const boost::system::error_code& err )
{
    con->listen();
    m_connection = con;
}

void tcp_client::write( const std::string& data)
{
    m_connection->write(data);
}

//----------------------------------------------------------------------------------- PRIMITIVES

int pyr_tcp_server_instantiate_run(VMGlobals* g, int n)
{
    auto server = new tcp_server( g_ioctx, 5678 );

    auto* ptr = slotRawObject(g->sp-1)->slots;
    SetPtr(ptr, server);

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
    // returns tcp_client pointer data
    return errNone;
}

int pyr_tcp_client_connect(VMGlobals* g, int n)
{
    // returns tcp_connection pointer data
    return errNone;
}

int pyr_tcp_client_disconnect(VMGlobals* g, int n)
{
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
    definePrimitive( base, index++, "_TcpClientConnect", pyr_tcp_client_connect, 2, 0);
    definePrimitive( base, index++, "_TcpClientDisconnect", pyr_tcp_client_disconnect, 2, 0);
}
