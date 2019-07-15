#include "PyrOssiaPrim.h"

#define strmaxle 4096
#define errpost_return(err) std::cout << err.message() << std::endl; return;

// ------------------------------------------------------------------------------------------------
extern bool compiledOK;
extern boost::asio::io_context ioService;
using  boost::asio::ip::tcp;
using namespace sclang;

// ------------------------------------------------------------------------------------------------
template<> inline bool
sclang::read( pyrslot* s) { return s->tag == tagTrue; }

template<> inline float
sclang::read(pyrslot* s) { return static_cast<float>(s->u.f); }

template<> inline int
sclang::read(pyrslot* s) { return static_cast<int>(s->u.i); }

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
    gMainVMGlobals->gc->Free(slotRawObject( s ));
    SetNil( s );
    delete data;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------

// ------------------------------------------------------------------------------------------------
// CONNECTION_PRIMITIVES
// ------------------------------------------------------------------------------------------------

int
pyr_ws_con_bind(VMGlobals* g, int)
{
    return errNone;
}

int
pyr_ws_con_write_text(VMGlobals* g, int)
{    
    return errNone;
}

WS_UNIMPLEMENTED int
pyr_ws_con_write_osc(VMGlobals* g, int)
{
    return errNone;
}

WS_UNIMPLEMENTED int
pyr_ws_con_write_binary(VMGlobals* g, int)
{
    return errNone;
}

WS_UNIMPLEMENTED int
pyr_ws_con_write_raw(VMGlobals* g, int)
{
    return errNone;
}

// ------------------------------------------------------------------------------------------------
// CLIENT_PRIMITIVES
// ------------------------------------------------------------------------------------------------

int
pyr_ws_client_create(VMGlobals* g, int)
{
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

int
pyr_ws_client_free(VMGlobals* g, int)
{
    return errNone;
}

// ------------------------------------------------------------------------------------------------
// SERVER_PRIMITIVES
// ------------------------------------------------------------------------------------------------

int
pyr_ws_server_instantiate_run(VMGlobals* g, int)
{
    return errNone;
}

int
pyr_ws_server_free(VMGlobals* g, int)
{
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
{
    int base = nextPrimitiveIndex(), index = 0;

    WS_DECLPRIM  ("_WebSocketConnectionWriteText", pyr_ws_con_write_text, 2);
    WS_DECLPRIM  ("_WebSocketConnectionWriteOSC", pyr_ws_con_write_osc, 2);
    WS_DECLPRIM  ("_WebSocketConnectionWriteBinary", pyr_ws_con_write_binary, 2);
    WS_DECLPRIM  ("_WebSocketConnectionWriteRaw", pyr_ws_con_write_raw, 2);
    WS_DECLPRIM  ("_WebSocketConnectionBind", pyr_ws_con_bind, 1);

    WS_DECLPRIM  ("_WebSocketClientCreate", pyr_ws_client_create, 1);
    WS_DECLPRIM  ("_WebSocketClientConnect", pyr_ws_client_connect, 3);
    WS_DECLPRIM  ("_WebSocketClientDisconnect", pyr_ws_client_disconnect, 1);
    WS_DECLPRIM  ("_WebSocketClientFree", pyr_ws_client_free, 1);

    WS_DECLPRIM  ("_WebSocketServerInstantiateRun", pyr_ws_server_instantiate_run, 2);
    WS_DECLPRIM  ("_WebSocketServerFree", pyr_ws_server_free, 1);

}
