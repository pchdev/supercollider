#include "PyrOssiaPrim.h"

#define strmaxle 4096
#define errpost_return(err) std::cout << err.message() << std::endl; return;

// ------------------------------------------------------------------------------------------------
extern bool compiledOK;
using namespace sclang;

// ------------------------------------------------------------------------------------------------
template<> inline bool
sclang::read(pyrslot* s) { return s->tag == tagTrue; }

template<> inline float
sclang::read(pyrslot* s) { return static_cast<float>(s->u.f); }

template<> inline int
sclang::read(pyrslot* s) { return static_cast<int>(s->u.i); }

template<typename T> inline T
sclang::read(pyrslot* s) { return static_cast<T>(slotRawPtr(s)); }

// ------------------------------------------------------------------------------------------------
template<> inline std::string
sclang::read(pyrslot* s)
// ------------------------------------------------------------------------------------------------
{
    char v[strmaxle];
    slotStrVal(s, v, strmaxle);
    return static_cast<std::string>(v);
}

// ------------------------------------------------------------------------------------------------
template<typename T> inline T
sclang::read(pyrslot* s, uint16_t index)
{
    return static_cast<T>(slotRawPtr(&slotRawObject(s)->slots[index]));
}

// ------------------------------------------------------------------------------------------------
template<> inline void
sclang::write(pyrslot* s, int v) { SetInt(s, v); }

template<> inline void
sclang::write(pyrslot* s, float v) { SetFloat(s, v); }

template<> inline void
sclang::write(pyrslot* s, double v) { SetFloat(s, v); }

template<> inline void
sclang::write(pyrslot* s, void* v) { SetPtr(s, v); }

template<> inline void
sclang::write(pyrslot* s, bool v) { SetBool(s, v); }

template<> inline void
sclang::write(pyrslot* s, pyrobject* o) { SetObject(s, o); }

template<typename T> inline void
sclang::write(pyrslot* s, T o) { SetPtr(s, o); }

// ------------------------------------------------------------------------------------------------
template<> inline
void sclang::write(pyrslot* s, std::string v)
// ------------------------------------------------------------------------------------------------
{
    PyrString* str = newPyrString(gMainVMGlobals->gc, v.c_str(), 0, true);
    SetObject(s, str);
}

// ------------------------------------------------------------------------------------------------
template<typename T> inline void
sclang::write(pyrslot* s, T object, uint16_t index )
// ------------------------------------------------------------------------------------------------
{
    pyrslot* ivar = slotRawObject(s)->slots+index;
    SetPtr(ivar, object);
}

// ------------------------------------------------------------------------------------------------
template<> inline void
sclang::write(pyrslot* s, int object, uint16_t index)
// ------------------------------------------------------------------------------------------------
{
    sclang::write(slotRawObject(s)->slots+index, object);
}

// ------------------------------------------------------------------------------------------------
template<> inline void
sclang::write(pyrslot* s, std::string object, uint16_t index)
// ------------------------------------------------------------------------------------------------
{
    pyrslot* ivar = slotRawObject(s)->slots+index;
    PyrString* str = newPyrString(gMainVMGlobals->gc, object.c_str(), 0, true);
    SetObject(ivar, str);
}

// ------------------------------------------------------------------------------------------------
template<typename T> void
sclang::return_data(pyrobject* object, T data, const char* sym)
// ------------------------------------------------------------------------------------------------
{
    gLangMutex.lock();

    if (compiledOK) {
        auto g = gMainVMGlobals;
        g->canCallOS = true;
        ++g->sp; sclang::write<pyrobject*>(g->sp, object);
        ++g->sp; sclang::write<T>(g->sp, data);
        runInterpreter(g, getsym(sym), 2);
        g->canCallOS = false;
    }

    gLangMutex.unlock();
}

// ------------------------------------------------------------------------------------------------
template<typename T> void
sclang::return_data(pyrobject* object, std::vector<T> data, const char* sym)
// ------------------------------------------------------------------------------------------------
{
    gLangMutex.lock();

    if (compiledOK) {
        auto g = gMainVMGlobals;
        g->canCallOS = true;
        ++g->sp; sclang::write<pyrobject*>(g->sp, object);
        for (const auto& d : data) {
              ++g->sp; sclang::write<T>(g->sp, d);
        }

        runInterpreter(g, getsym(sym), data.size()+1);
        g->canCallOS = false;
    }

    gLangMutex.unlock();
}

// ------------------------------------------------------------------------------------------------
template<typename T> inline void
sclang::free(pyrslot* s, T data)
// ------------------------------------------------------------------------------------------------
{
    gMainVMGlobals->gc->Free(slotRawObject(s));
    SetNil(s);
    delete data;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------

// ------------------------------------------------------------------------------------------------
// CONNECTION_PRIMITIVES
// ------------------------------------------------------------------------------------------------

// ------------------------------------------------------------------------------------------------
int
pyr_ws_con_bind(VMGlobals* g, int)
// ------------------------------------------------------------------------------------------------
{
    auto nc     = sclang::read<network::Connection*>(g->sp);
    auto mgc    = sclang::read<mg_connection*>(g->sp, 0);

    // write address/port in sc object
    char addr[32];
    mg_sock_addr_to_str(&mgc->sa, addr, sizeof(addr), MG_SOCK_STRINGIFY_IP);
    std::string saddr(addr, 32);
    sclang::write(g->sp, saddr, 1);

    char s_port[8];
    mg_sock_addr_to_str(&mgc->sa, s_port, sizeof(s_port), MG_SOCK_STRINGIFY_PORT);
    std::string strport(s_port, 8);
    int port = std::stoi(strport);
    sclang::write<int>(g->sp, port, 2);

    nc->object = slotRawObject(g->sp);
    return errNone;
}

// ------------------------------------------------------------------------------------------------
int
pyr_ws_con_write_text(VMGlobals* g, int)
// ------------------------------------------------------------------------------------------------
{
    auto nc     = sclang::read<network::Connection*>(g->sp-1, 0);
    auto text   = sclang::read<std::string>(g->sp);

    mg_send_websocket_frame(nc->connection, WEBSOCKET_OP_TEXT, text.c_str(), text.size());
    return errNone;
}

// ------------------------------------------------------------------------------------------------
int
pyr_ws_con_write_osc(VMGlobals* g, int)
// that would be a variant array
// see oscpack for parsing
// ------------------------------------------------------------------------------------------------
{
    auto connection = sclang::read<network::Connection*>(g->sp-1, 0);
    return errNone;
}

// ------------------------------------------------------------------------------------------------
int
pyr_ws_con_write_binary(VMGlobals* g, int)
// ------------------------------------------------------------------------------------------------
{
    auto connection = sclang::read<network::Connection*>(g->sp-1, 0);
    return errNone;
}

// ------------------------------------------------------------------------------------------------
// CLIENT_PRIMITIVES
// ------------------------------------------------------------------------------------------------

// ------------------------------------------------------------------------------------------------
int
pyr_ws_client_create(VMGlobals* g, int)
// ------------------------------------------------------------------------------------------------
{
    auto host = sclang::read<std::string>(g->sp-1);
    auto port = sclang::read<int>(g->sp);

    auto client = new network::Client(host, port);
    sclang::write(g->sp-2, client);

    return errNone;
}

int
pyr_ws_client_connect(VMGlobals* g, int)
{
    return errNone;
}

int
pyr_ws_client_disconnect(VMGlobals* g, int)
{
    return errNone;
}

// ------------------------------------------------------------------------------------------------
int
pyr_ws_client_free(VMGlobals* g, int)
// ------------------------------------------------------------------------------------------------
{
    auto client = sclang::read<network::Client*>(g->sp, 0);
    sclang::free(g->sp, client);

    return errNone;
}

// ------------------------------------------------------------------------------------------------
// SERVER_PRIMITIVES

// ------------------------------------------------------------------------------------------------
int
pyr_ws_server_instantiate_run(VMGlobals* g, int)
// ------------------------------------------------------------------------------------------------
{
    int port = sclang::read<int>(g->sp);
    auto server = new network::Server(port);
    server->object = slotRawObject(g->sp-1);

    sclang::write(g->sp-1, server, 0);
    return errNone;
}

// ------------------------------------------------------------------------------------------------
int
pyr_ws_server_free(VMGlobals* g, int)
// ------------------------------------------------------------------------------------------------
{
    auto server = sclang::read<network::Server*>(g->sp, 0);
    sclang::free(g->sp, server);

    return errNone;
}

// ------------------------------------------------------------------------------------------------
int
pyr_http_request_bind(vmglobals* g, int)
// loads contents of request in the sc object's instvariables
// ------------------------------------------------------------------------------------------------
{
    auto req = sclang::read<network::HttpRequest*>(g->sp, 0);
    auto hm = req->message;

    std::string method(hm->uri.p, hm->uri.len);
    std::string query(hm->query_string.p, hm->query_string.len);
//    std::string mime; // todo

    std::string contents(hm->body.p, hm->body.len);

    sclang::write(g->sp, method, 1);
    sclang::write(g->sp, query, 2);
    sclang::write(g->sp, contents, 4);

    return errNone;
}

// ------------------------------------------------------------------------------------------------
int
pyr_http_send(vmglobals* g, int)
// ------------------------------------------------------------------------------------------------
{
    return errNone;
}

// ------------------------------------------------------------------------------------------------
int
pyr_http_reply(vmglobals* g, int)
// ------------------------------------------------------------------------------------------------
{
    auto req = sclang::read<network::HttpRequest*>(g->sp-3, 0);
    auto code = sclang::read<int>(g->sp-2);
    auto body = sclang::read<std::string>(g->sp-1);
    auto mime = sclang::read<std::string>(g->sp);

    if (!mime.empty())
         mime.append("Content-Type: ");

    mg_send_head(req->connection, code, mime.length(), mime.data());
    mg_send(req->connection, body.data(), body.length());

    return errNone;
}

// ------------------------------------------------------------------------------------------------
// PRIMITIVES_INITIALIZATION
//---------------------------
#define WS_DECLPRIM(_s, _f, _n)                     \
definePrimitive( base, index++, _s, _f, _n, 0)

// ------------------------------------------------------------------------------------------------
void
network::initialize()
// ------------------------------------------------------------------------------------------------
{
    int base = nextPrimitiveIndex(), index = 0;

    WS_DECLPRIM  ("_WebSocketConnectionWriteText", pyr_ws_con_write_text, 2);
    WS_DECLPRIM  ("_WebSocketConnectionWriteOsc", pyr_ws_con_write_osc, 2);
    WS_DECLPRIM  ("_WebSocketConnectionWriteBinary", pyr_ws_con_write_binary, 2);
    WS_DECLPRIM  ("_WebSocketConnectionBind", pyr_ws_con_bind, 1);

    WS_DECLPRIM  ("_WebSocketClientCreate", pyr_ws_client_create, 1);
    WS_DECLPRIM  ("_WebSocketClientConnect", pyr_ws_client_connect, 3);
    WS_DECLPRIM  ("_WebSocketClientDisconnect", pyr_ws_client_disconnect, 1);
    WS_DECLPRIM  ("_WebSocketClientRequest", pyr_http_send, 1);
    WS_DECLPRIM  ("_WebSocketClientFree", pyr_ws_client_free, 1);

    WS_DECLPRIM  ("_WebSocketServerInstantiateRun", pyr_ws_server_instantiate_run, 2);
    WS_DECLPRIM  ("_WebSocketServerFree", pyr_ws_server_free, 1);

    WS_DECLPRIM  ("_HttpRequestBind", pyr_http_request_bind, 1);
    WS_DECLPRIM  ("_HttpReply", pyr_http_reply, 3);

}
