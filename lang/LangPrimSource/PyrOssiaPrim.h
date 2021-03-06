#pragma once

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
#include <avahi-common/simple-watch.h>
#endif

#include "PyrSymbolTable.h"
#include "PyrSched.h"
#include "PyrPrimitive.h"
#include "PyrKernel.h"
#include "PyrSymbol.h"
#include "PyrInterpreter.h"
#include "GC.h"
#include "SC_LanguageClient.h"
#include "websocket/mongoose.h"
#include <iostream>

// ------------------------------------------------------------------------------------------------
using pyrslot   = PyrSlot;
using pyrobject = PyrObject;
using vmglobals = VMGlobals;
using pyrint8array = PyrInt8Array;

// ------------------------------------------------------------------------------------------------
#define TCP_BUFSIZE 32768
#define WS_GENERIC_T template<typename T>
#define WS_OPTIMIZE
#define WS_REFACTOR
#define WS_UNIMPLEMENTED

// ------------------------------------------------------------------------------------------------
// SCLANG-GENERIC-UTILITIES
namespace sclang {

template<typename T> void
return_data(pyrobject* object, T data, const char* sym);

// ------------------------------------------------------------------------------------------------
template<typename T> void
return_data(pyrobject* object, std::vector<T>, const char* sym);
// calls 'sym' sc-method, passing mutiple data as arguments

// ------------------------------------------------------------------------------------------------
template<typename T> void
write(pyrslot* s, T object);
// pushes object 'T' to slot 's'

// ------------------------------------------------------------------------------------------------
template<typename T> void
write(pyrslot* s, T object, uint16_t index);
// pushes object 'T' to  object's instvar 'index'

// ------------------------------------------------------------------------------------------------
template<typename T> T
read(pyrslot* s, uint16_t index);
// reads object 'T' from object's instvar 'index'

// ------------------------------------------------------------------------------------------------
template<typename T> T
read(pyrslot* s);
// reads object 'T' from slot 's'

// ------------------------------------------------------------------------------------------------
template<typename T> void
free(pyrslot* s, T object);
// frees object from slot and heap
}

// ------------------------------------------------------------------------------------------------
// NETWORK-OBSERVERS
namespace network {

using avahi_client = AvahiClient;
using avahi_simple_poll = AvahiSimplePoll;
using avahi_entry_group = AvahiEntryGroup;

// ------------------------------------------------------------------------------------------------
void
initialize();

// ------------------------------------------------------------------------------------------------
struct Connection
// ------------------------------------------------------------------------------------------------
{
    pyrobject*
    object = nullptr;

    mg_connection*
    connection = nullptr;

    // ------------------------------------------------------------------------------------------------
    Connection(mg_connection* mgc) : connection(mgc) {}

    // ------------------------------------------------------------------------------------------------
    bool
    operator==(Connection const& rhs) { return connection == rhs.connection; }

    bool
    operator==(mg_connection* rhs) { return connection == rhs; }
};

// ------------------------------------------------------------------------------------------------
struct HttpRequest
// ------------------------------------------------------------------------------------------------
{
    pyrobject*
    object = nullptr;

    mg_connection*
    connection = nullptr;

    http_message*
    message = nullptr;

    // ------------------------------------------------------------------------------------------------
    HttpRequest(mg_connection* con, http_message* msg) :
        connection(con), message(msg) {}
};

// ------------------------------------------------------------------------------------------------
class Server
// ------------------------------------------------------------------------------------------------
{
    avahi_simple_poll*
    m_avpoll = nullptr;

    avahi_entry_group*
    m_avgroup = nullptr;

    avahi_client*
    m_avclient = nullptr;

    std::vector<Connection>
    m_connections;

    mg_mgr
    m_mginterface;

    pthread_t
    m_mgthread,
    m_avthread;

    uint16_t
    m_port = 5678;

    std::string
    m_name,
    m_type;

    bool
    m_running = false;

public:

    pyrobject*
    object = nullptr;

    // ------------------------------------------------------------------------------------------------
    Server(uint16_t port, std::string zcname, std::string zctype) :
        m_port(port),
        m_name(zcname),
        m_type(zctype)
    {
        initialize();
    }

    // ------------------------------------------------------------------------------------------------
    void
    initialize()
    // ------------------------------------------------------------------------------------------------
    {
        mg_mgr_init(&m_mginterface, this);
        char s_tcp[5];
        sprintf(s_tcp, "%d", m_port);

        fprintf(stdout, "[websocket] binding server on port %d\n", m_port);

        auto connection = mg_bind(&m_mginterface, s_tcp, ws_event_handler);
        mg_set_protocol_http_websocket(connection);

        fprintf(stdout, "[avahi] registering service: %s\n", m_name.c_str());

        int err     = 0;
        m_avpoll    = avahi_simple_poll_new();
        m_avclient  = avahi_client_new(avahi_simple_poll_get(m_avpoll),
                      static_cast<AvahiClientFlags>(0), avahi_client_callback, this, &err);

        if (err) {
            fprintf(stdout, "[avahi] error creating new client: %d\n", err);
            // memo -26 = daemon not running,
            // with systemd, just do $systemctl enable avahi-daemon.service
        }

        m_running = true;
        poll();
    }

    // ------------------------------------------------------------------------------------------------
    ~Server()
    // ------------------------------------------------------------------------------------------------
    {
        m_running = false;
        pthread_join(m_mgthread, nullptr);
        pthread_join(m_avthread, nullptr);

        avahi_client_free(m_avclient);
        avahi_simple_poll_free(m_avpoll);

        mg_mgr_free(&m_mginterface);
    }

    //-------------------------------------------------------------------------------------------------
    void
    poll()
    //-------------------------------------------------------------------------------------------------
    {
        pthread_create(&m_mgthread, nullptr, pthread_server_poll, this);
        pthread_create(&m_avthread, nullptr, pthread_avahi_poll, this);
    }

    //-------------------------------------------------------------------------------------------------
    static void*
    pthread_server_poll(void* udata)
    //-------------------------------------------------------------------------------------------------
    {
        auto server = static_cast<Server*>(udata);
        while (server->m_running)
            mg_mgr_poll(&server->m_mginterface, 200);

        return nullptr;
    }

    //-------------------------------------------------------------------------------------------------
    static void*
    pthread_avahi_poll(void* udata)
    //-------------------------------------------------------------------------------------------------
    {
        auto server = static_cast<Server*>(udata);
        avahi_simple_poll_loop(server->m_avpoll);
        return nullptr;
    }

    //-------------------------------------------------------------------------------------------------
    static void
    avahi_group_callback(avahi_entry_group* group, AvahiEntryGroupState state, void* udata)
    //-------------------------------------------------------------------------------------------------
    {
        switch(state)
        {
        case AVAHI_ENTRY_GROUP_REGISTERING:
        case AVAHI_ENTRY_GROUP_ESTABLISHED:
        case AVAHI_ENTRY_GROUP_UNCOMMITED:
            break;
        case AVAHI_ENTRY_GROUP_COLLISION:
        {
            fprintf(stdout, "[avahi] entry group collision\n");
            break;
        }
        case AVAHI_ENTRY_GROUP_FAILURE:
        {
            fprintf(stdout, "[avahi] entry group failure\n");
            break;
        }

        }
    }

    //-------------------------------------------------------------------------------------------------
    static void
    avahi_client_callback(avahi_client* client, AvahiClientState state, void* udata)
    //-------------------------------------------------------------------------------------------------
    {
        auto server = static_cast<Server*>(udata);

        switch(state)
        {
        case AVAHI_CLIENT_CONNECTING:
        case AVAHI_CLIENT_S_REGISTERING:
        case AVAHI_CLIENT_S_RUNNING:
        {
            fprintf(stdout, "[avahi] client running\n");

            auto group = server->m_avgroup;
            if(!group)
            {
                fprintf(stdout, "[avahi] creating entry group\n");
                group  = avahi_entry_group_new(client, avahi_group_callback, server);
                server->m_avgroup = group;
            }

            if (avahi_entry_group_is_empty(group))
            {
                fprintf(stdout, "[avahi] adding service\n");

                int err = avahi_entry_group_add_service(group,
                    AVAHI_IF_UNSPEC, AVAHI_PROTO_INET, static_cast<AvahiPublishFlags>(0),
                    server->m_name.c_str(), server->m_type.c_str(),
                    nullptr, nullptr, server->m_port, nullptr);

                if (err) {
                     fprintf(stdout, "Failed to add service: %s\n", avahi_strerror(err));
                     return;
                }

                fprintf(stdout, "[avahi] commiting service\n");
                err = avahi_entry_group_commit(group);

                if (err) {
                    fprintf(stdout, "Failed to commit group: %s\n", avahi_strerror(err));
                    return;
                }
            }
            break;
        }
        case AVAHI_CLIENT_FAILURE:
        {
            fprintf(stdout, "[avahi] client failure");
            break;
        }
        case AVAHI_CLIENT_S_COLLISION:
        {
            fprintf(stdout, "[avahi] client collision");
            break;
        }
        }
    }

    //-------------------------------------------------------------------------------------------------
    static void
    ws_event_handler(mg_connection* mgc, int event, void* data);
};

// ------------------------------------------------------------------------------------------------
class Client
// ------------------------------------------------------------------------------------------------
{
    Connection
    m_connection;

    pthread_t
    m_thread;

    mg_mgr
    m_ws_mgr,
    m_http_mgr;

    std::string
    m_host;

    uint16_t
    m_port = 0;

    bool
    m_running = false;

public:

    pyrobject*
    object = nullptr;

    // ------------------------------------------------------------------------------------------------
    Client() : m_connection(nullptr)
    // ------------------------------------------------------------------------------------------------
    {
        mg_mgr_init(&m_ws_mgr, this);
        mg_mgr_init(&m_http_mgr, this);
    }

    // ------------------------------------------------------------------------------------------------
    void
    connect(std::string host, uint16_t port)
    // ------------------------------------------------------------------------------------------------
    {
        m_host = host;
        m_port = port;

        std::string ws_addr("ws://");
        ws_addr.append(host);
        ws_addr.append(":");
        ws_addr.append(std::to_string(port));

        m_connection.connection = mg_connect_ws(&m_ws_mgr, event_handler, ws_addr.c_str(), nullptr, nullptr);
        assert(m_connection.connection); //for now

        m_running = true;
        pthread_create(&m_thread, nullptr, pthread_client_poll, this);
    }

    // ------------------------------------------------------------------------------------------------
    void
    request(std::string req)
    // ------------------------------------------------------------------------------------------------
    {
        std::string addr(m_host);
        addr.append(":");
        addr.append(std::to_string(m_port));
        addr.append(req);

        auto mgc = mg_connect_http(&m_ws_mgr, event_handler, addr.data(), nullptr, nullptr);
    }

    // ------------------------------------------------------------------------------------------------
    ~Client()
    // ------------------------------------------------------------------------------------------------
    {
        pthread_join(m_thread, nullptr);
        mg_mgr_free(&m_ws_mgr);
        mg_mgr_free(&m_http_mgr);
    }

    //-------------------------------------------------------------------------------------------------
    static void*
    pthread_client_poll(void* udata)
    //-------------------------------------------------------------------------------------------------
    {
        auto client = static_cast<Client*>(udata);
        while (client->m_running) {
            mg_mgr_poll(&client->m_ws_mgr, 200);
//            mg_mgr_poll(&client->m_http_mgr, 200);
        }

        return nullptr;
    }

    // ------------------------------------------------------------------------------------------------
    static void
    event_handler(mg_connection* mgc, int event, void* data);

};

}
