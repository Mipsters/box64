#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <dlfcn.h>

#include "wrappedlibs.h"

#include "debug.h"
#include "wrapper.h"
#include "bridge.h"
#include "librarian/library_private.h"
#include "x64emu.h"
#include "emu/x64emu_private.h"
#include "callback.h"
#include "librarian.h"
#include "box64context.h"
#include "emu/x64emu_private.h"

const char* libxextName = "libXext.so.6";
#define LIBNAME libxext
static library_t* my_lib = NULL;

typedef struct _XImage XImage;
void BridgeImageFunc(x64emu_t *emu, XImage *img);
void UnbridgeImageFunc(x64emu_t *emu, XImage *img);
typedef int (*XextErrorHandler)(void *, void *, void*);

typedef struct my_XExtensionHooks {
    int (*create_gc)(void*, uint32_t, void*);
    int (*copy_gc)(void*, uint32_t, void*);
    int (*flush_gc)(void*, uint32_t, void*);
    int (*free_gc)(void*, uint32_t, void*);
    int (*create_font)(void*, void*, void*);
    int (*free_font)(void*, void*, void*);
    int (*close_display)(void*, void*);
    int (*wire_to_event)(void*, void*, void*);
    int (*event_to_wire)(void*, void*, void*);
    int (*error)(void*, void*, void*, int*);
    char *(*error_string)(void*, int, void*, void*, int);
} my_XExtensionHooks;


#include "generated/wrappedlibxexttypes.h"

typedef struct xext_my_s {
    // functions
    #define GO(A, B)    B   A;
    SUPER()
    #undef GO
} xext_my_t;

void* getXextMy(library_t* lib)
{
    xext_my_t* my = (xext_my_t*)calloc(1, sizeof(xext_my_t));
    #define GO(A, W) my->A = (W)dlsym(lib->priv.w.lib, #A);
    SUPER()
    #undef GO
    return my;
}

void freeXextMy(void* lib)
{
    (void)lib;
    //xext_my_t *my = (xext_my_t *)lib;
}
#undef SUPER

#define SUPER() \
GO(0)   \
GO(1)   \
GO(2)   \
GO(3)   \
GO(4)

// exterrorhandle ...
#define GO(A)   \
static uintptr_t my_exterrorhandle_fct_##A = 0;                                                 \
static int my_exterrorhandle_##A(void* display, void* ext_name, void* reason)                   \
{                                                                                               \
    return RunFunction(my_context, my_exterrorhandle_fct_##A, 3, display, ext_name, reason);    \
}
SUPER()
#undef GO
static void* find_exterrorhandle_Fct(void* fct)
{
    if(!fct) return fct;
    if(GetNativeFnc((uintptr_t)fct))  return GetNativeFnc((uintptr_t)fct);
    #define GO(A) if(my_exterrorhandle_fct_##A == (uintptr_t)fct) return my_exterrorhandle_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_exterrorhandle_fct_##A == 0) {my_exterrorhandle_fct_##A = (uintptr_t)fct; return my_exterrorhandle_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libXext exterrorhandle callback\n");
    return NULL;
}
static void* reverse_exterrorhandleFct(void* fct)
{
    if(!fct) return fct;
    if(CheckBridged(my_lib->priv.w.bridge, fct))
        return (void*)CheckBridged(my_lib->priv.w.bridge, fct);
    #define GO(A) if(my_exterrorhandle_##A == fct) return (void*)my_exterrorhandle_fct_##A;
    SUPER()
    #undef GO
    return (void*)AddBridge(my_lib->priv.w.bridge, iFppp, fct, 0, NULL);
}

#undef SUPER

EXPORT void* my_XShmCreateImage(x64emu_t* emu, void* disp, void* vis, uint32_t depth, int32_t fmt
                    , void* data, void* shminfo, uint32_t w, uint32_t h)
{
    xext_my_t *my = (xext_my_t*)my_lib->priv.w.p2;

    XImage *img = my->XShmCreateImage(disp, vis, depth, fmt, data, shminfo, w, h);
    if(!img)
        return img;
    // bridge all access functions...
    BridgeImageFunc(emu, img);
    return img;
}

EXPORT int32_t my_XShmPutImage(x64emu_t* emu, void* disp, void* drawable, void* gc, void* image
                    , int32_t src_x, int32_t src_y, int32_t dst_x, int32_t dst_y
                    , uint32_t w, uint32_t h, int32_t sendevt)
{
    xext_my_t *my = (xext_my_t*)my_lib->priv.w.p2;

    UnbridgeImageFunc(emu, (XImage*)image);
    int32_t r = my->XShmPutImage(disp, drawable, gc, image, src_x, src_y, dst_x, dst_y, w, h, sendevt);
    // bridge all access functions...
    BridgeImageFunc(emu, (XImage*)image);
    return r;
}

EXPORT int32_t my_XShmGetImage(x64emu_t* emu, void* disp, void* drawable, void* image, int32_t x, int32_t y, uint32_t plane)
{
    xext_my_t *my = (xext_my_t*)my_lib->priv.w.p2;

    UnbridgeImageFunc(emu, (XImage*)image);
    int32_t r = my->XShmGetImage(disp, drawable, image, x, y, plane);
    // bridge all access functions...
    BridgeImageFunc(emu, (XImage*)image);
    return r;
}

EXPORT void* my_XSetExtensionErrorHandler(x64emu_t* emu, void* handler)
{
    (void)emu;
    xext_my_t *my = (xext_my_t*)my_lib->priv.w.p2;
    return reverse_exterrorhandleFct(my->XSetExtensionErrorHandler(find_exterrorhandle_Fct(handler)));
}

static box64context_t *context = NULL;
static uintptr_t my_hook_create_gc_fnc = 0;
static uintptr_t my_hook_copy_gc_fnc = 0;
static uintptr_t my_hook_flush_gc_fnc = 0;
static uintptr_t my_hook_free_gc_fnc = 0;
static uintptr_t my_hook_create_font_fnc = 0;
static uintptr_t my_hook_free_font_fnc = 0;
static uintptr_t my_hook_close_display_fnc = 0;
static uintptr_t my_hook_wire_to_event_fnc = 0;
static uintptr_t my_hook_event_to_wire_fnc = 0;
static uintptr_t my_hook_error_fnc = 0;
static uintptr_t my_hook_error_string_fnc = 0;
static int  my_hook_create_gc(void* a, uint32_t b, void* c) {
    if(my_hook_create_gc_fnc)
        return (int)RunFunction(context, my_hook_create_gc_fnc, 3, a, b, c);
    return 0;
}
static int  my_hook_copy_gc(void* a, uint32_t b, void* c) {
    if(my_hook_copy_gc_fnc)
        return (int)RunFunction(context, my_hook_copy_gc_fnc, 3, a, b, c);
    return 0;
}
static int  my_hook_flush_gc(void* a, uint32_t b, void* c) {
    if(my_hook_flush_gc_fnc)
        return (int)RunFunction(context, my_hook_flush_gc_fnc, 3, a, b, c);
    return 0;
}
static int  my_hook_free_gc(void* a, uint32_t b, void* c) {
    if(my_hook_free_gc_fnc)
        return (int)RunFunction(context, my_hook_free_gc_fnc, 3, a, b, c);
    return 0;
}
static int  my_hook_create_font(void* a, void* b, void* c) {
    if(my_hook_create_font_fnc)
        return (int)RunFunction(context, my_hook_create_font_fnc, 3, a, b, c);
    return 0;
}
static int  my_hook_free_font(void* a, void* b, void* c) {
    if(my_hook_free_font_fnc)
        return (int)RunFunction(context, my_hook_free_font_fnc, 3, a, b, c);
    return 0;
}
static int  my_hook_close_display(void* a, void* b) {
    if(my_hook_close_display_fnc)
        return (int)RunFunction(context, my_hook_close_display_fnc, 2, a, b);
    return 0;
}
static int  my_hook_wire_to_event(void* a, void* b, void* c) {
    if(my_hook_wire_to_event_fnc)
        return (int)RunFunction(context, my_hook_wire_to_event_fnc, 3, a, b, c);
    return 0;
}
static int  my_hook_event_to_wire(void* a, void* b, void* c) {
    if(my_hook_event_to_wire_fnc)
        return (int)RunFunction(context, my_hook_event_to_wire_fnc, 3, a, b, c);
    return 0;
}
static int  my_hook_error(void* a, void* b, void* c, int* d) {
    if(my_hook_error_fnc)
        return (int)RunFunction(context, my_hook_error_fnc, 4, a, b, c, d);
    return 0;
}
static char* my_hook_error_string(void* a, int b, void* c, void* d, int e) {
    if(my_hook_error_string_fnc)
        return (char*)RunFunction(context, my_hook_error_string_fnc, 5, a, b, c, d, e);
    return 0;
}

EXPORT void* my_XextAddDisplay(x64emu_t* emu, void* extinfo, void* dpy, void* extname, my_XExtensionHooks* hooks, int nevents, void* data)
{
    xext_my_t *my = (xext_my_t*)my_lib->priv.w.p2;

    if(!context)
        context = emu->context;

    my_XExtensionHooks natives = {0};
    #define GO(A) if(hooks->A) {my_hook_##A##_fnc = (uintptr_t)hooks->A; natives.A = my_hook_##A;}
    GO(create_gc)
    GO(copy_gc)
    GO(flush_gc)
    GO(free_gc)
    GO(create_font)
    GO(free_font)
    GO(close_display)
    GO(wire_to_event)
    GO(event_to_wire)
    GO(error)
    GO(error_string)
    #undef GO
    void *ret = my->XextAddDisplay(extinfo, dpy, extname, &natives, nevents, data);
    return ret;
}

#define CUSTOM_INIT \
    lib->priv.w.p2 = getXextMy(lib);    \
    my_lib = lib;                       \
    lib->priv.w.needed = 5;             \
    lib->priv.w.neededlibs = (char**)calloc(lib->priv.w.needed, sizeof(char*)); \
    lib->priv.w.neededlibs[0] = strdup("libX11.so.6"); \
    lib->priv.w.neededlibs[1] = strdup("libxcb.so.1"); \
    lib->priv.w.neededlibs[2] = strdup("libXau.so.6"); \
    lib->priv.w.neededlibs[3] = strdup("libdl.so.2");  \
    lib->priv.w.neededlibs[4] = strdup("libXdmcp.so.6");

#define CUSTOM_FINI \
    freeXextMy(lib->priv.w.p2); \
    free(lib->priv.w.p2);       \
    my_lib = NULL;

#include "wrappedlib_init.h"
