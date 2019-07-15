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

#include <mongoose.h>

// ------------------------------------------------------------------------------------------------
using pyrslot   = PyrSlot;
using pyrobject = PyrObject;
using vmglobals = VMGlobals;

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
class Connection
// ------------------------------------------------------------------------------------------------
{
    mg_connection*
    m_connection = nullptr;

public:
    Connection();

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
    m_name;

    bool
    m_running = false;

public:

    // ------------------------------------------------------------------------------------------------
    Server() { initialize(); }

    // ------------------------------------------------------------------------------------------------
    Server(uint16_t port) : m_port(port) { initialize(); }

    // ------------------------------------------------------------------------------------------------
    void
    initialize()
    // ------------------------------------------------------------------------------------------------
    {
        mg_mgr_init(&m_mginterface, this);
        char s_tcp[5];
        sprintf(s_tcp, "%d", m_port);

        auto tcp_connection = mg_bind(&m_mginterface, s_tcp, ws_event_handler);
        mg_set_protocol_http_websocket(tcp_connection);

        m_avpoll    = avahi_simple_poll_new();
        m_avclient  = avahi_client_new(avahi_simple_poll_get(m_avpoll),
                      static_cast<AvahiClientFlags>(0), avahi_client_callback, this, nullptr);

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
        {
            fprintf(stderr, "[avahi] entry group registering\n");
            break;
        }
        case AVAHI_ENTRY_GROUP_ESTABLISHED:
        {
            fprintf(stderr, "[avahi] entry group established\n");
            break;
        }
        case AVAHI_ENTRY_GROUP_COLLISION:
        {
            fprintf(stderr, "[avahi] entry group collision\n");
            break;
        }
        case AVAHI_ENTRY_GROUP_FAILURE:
        {
            fprintf(stderr, "[avahi] entry group failure\n");
            break;
        }
        case AVAHI_ENTRY_GROUP_UNCOMMITED:
        {
            fprintf(stderr, "[avahi] entry group uncommited\n");
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
        {
            fprintf(stderr, "[avahi] client connecting\n");
            break;
        }
        case AVAHI_CLIENT_S_REGISTERING:
        {
            fprintf(stderr, "[avahi] client registering\n");
            break;
        }
        case AVAHI_CLIENT_S_RUNNING:
        {
            fprintf(stderr, "[avahi] client running\n");

            auto group = server->m_avgroup;
            if(!group)
            {
                fprintf(stderr, "[avahi] creating entry group\n");
                group  = avahi_entry_group_new(client, avahi_group_callback, server);
                server->m_avgroup = group;
            }

            if (avahi_entry_group_is_empty(group))
            {
                fprintf(stderr, "[avahi] adding service\n");

                int err = avahi_entry_group_add_service(group,
                    AVAHI_IF_UNSPEC, AVAHI_PROTO_INET, static_cast<AvahiPublishFlags>(0),
                    server->m_name.c_str(), "_oscjson._tcp", nullptr, nullptr, server->m_port, nullptr);

                if (err) {
                     fprintf(stderr, "Failed to add service: %s\n", avahi_strerror(err));
                     return;
                }

                fprintf(stderr, "[avahi] commiting service\n");
                err = avahi_entry_group_commit(group);

                if (err) {
                    fprintf(stderr, "Failed to commit group: %s\n", avahi_strerror(err));
                    return;
                }
            }
            break;
        }
        case AVAHI_CLIENT_FAILURE:
        {
            fprintf(stderr, "[avahi] client failure");
            break;
        }
        case AVAHI_CLIENT_S_COLLISION:
        {
            fprintf(stderr, "[avahi] client collision");
            break;
        }
        }
    }

    //-------------------------------------------------------------------------------------------------
    static void
    ws_event_handler(mg_connection* mgc, int event, void* data)
    //-------------------------------------------------------------------------------------------------
    {
        auto server = static_cast<Server*>(mgc->mgr->user_data);

        switch(event)
        {
        case MG_EV_RECV:
        {
            break;
        }
        case MG_EV_WEBSOCKET_HANDSHAKE_DONE:
        {
            break;
        }
        case MG_EV_WEBSOCKET_FRAME:
        {
            break;
        }
        case MG_EV_HTTP_REQUEST:
        {
            http_message* hm = static_cast<http_message*>(data);
            break;
        }
        case MG_EV_CLOSE:
        {
            break;
        }
        }
    }
};

// ------------------------------------------------------------------------------------------------
class Client
// ------------------------------------------------------------------------------------------------
{
    mg_connection*
    m_connection;

    pthread_t
    m_thread;

    mg_mgr
    m_mgr;

    std::string
    m_host;

    uint16_t
    m_port = 0;

public:

    // ------------------------------------------------------------------------------------------------
    Client()
    // ------------------------------------------------------------------------------------------------
    {

    }

    // ------------------------------------------------------------------------------------------------
    Client(std::string hostaddr, uint16_t port) : m_host(hostaddr), m_port(port)
    // ------------------------------------------------------------------------------------------------
    {

    }

    // ------------------------------------------------------------------------------------------------
    ~Client()
    // ------------------------------------------------------------------------------------------------
    {

    }

};

}
