#define _LARGEFILE_SOURCE 1
#define _FILE_OFFSET_BITS 64
#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <dlfcn.h>

#ifdef __BIONIC__

#ifdef __PTRDIFF_TYPE__
# define PTR_INT_TYPE __PTRDIFF_TYPE__
#else
# include <stddef.h>
# define PTR_INT_TYPE ptrdiff_t
#endif

struct _obstack_chunk           /* Lives at front of each chunk. */
{
  char *limit;                  /* 1 past end of this chunk */
  struct _obstack_chunk *prev;  /* address of prior chunk or NULL */
  char contents[4];             /* objects begin here */
};

struct obstack          /* control current object in current chunk */
{
  long chunk_size;              /* preferred size to allocate chunks in */
  struct _obstack_chunk *chunk; /* address of current struct obstack_chunk */
  char *object_base;            /* address of object we are building */
  char *next_free;              /* where to add next char to current object */
  char *chunk_limit;            /* address of char after current chunk */
  union
  {
    PTR_INT_TYPE tempint;
    void *tempptr;
  } temp;                       /* Temporary for some macros.  */
  int alignment_mask;           /* Mask of alignment for each object. */
  /* These prototypes vary based on 'use_extra_arg', and we use
     casts to the prototypeless function type in all assignments,
     but having prototypes here quiets -Wstrict-prototypes.  */
  struct _obstack_chunk *(*chunkfun) (void *, long);
  void (*freefun) (void *, struct _obstack_chunk *);
  void *extra_arg;              /* first arg for chunk alloc/dealloc funcs */
  unsigned use_extra_arg : 1;     /* chunk alloc/dealloc funcs take extra arg */
  unsigned maybe_empty_object : 1; /* There is a possibility that the current
                                      chunk contains a zero-length object.  This
                                      prevents freeing the chunk if we allocate
                                      a bigger chunk to replace it. */
  unsigned alloc_failed : 1;      /* No longer used, as we now call the failed
                                     handler on error, but retained for binary
                                     compatibility.  */
};

/* Determine default alignment.  */
union fooround
{
  uintmax_t i;
  long double d;
  void *p;
};
struct fooalign
{
  char c;
  union fooround u;
};
/* If malloc were really smart, it would round addresses to DEFAULT_ALIGNMENT.
   But in fact it might be less smart and round addresses to as much as
   DEFAULT_ROUNDING.  So we prepare for it to do that.  */
enum
{
  DEFAULT_ALIGNMENT = offsetof (struct fooalign, u),
  DEFAULT_ROUNDING = sizeof (union fooround)
};

/* The functions allocating more room by calling 'obstack_chunk_alloc'
    jump to the handler pointed to by 'obstack_alloc_failed_handler'.
    This can be set to a user defined function which should either
    abort gracefully or use longjump - but shouldn't return.  This
    variable by default points to the internal function
    'print_and_abort'.  */
 static _Noreturn void print_and_abort (void);
 void (*obstack_alloc_failed_handler) (void) = print_and_abort;

/* Define a macro that either calls functions with the traditional malloc/free
   calling interface, or calls functions with the mmalloc/mfree interface
   (that adds an extra first argument), based on the state of use_extra_arg.
   For free, do not use ?:, since some compilers, like the MIPS compilers,
   do not allow (expr) ? void : void.  */

# define CALL_CHUNKFUN(h, size) \
  (((h)->use_extra_arg)                                                       \
   ? (*(h)->chunkfun)((h)->extra_arg, (size))                                 \
   : (*(struct _obstack_chunk *(*)(long))(h)->chunkfun)((size)))

# define CALL_FREEFUN(h, old_chunk) \
  do { \
      if ((h)->use_extra_arg)                                                 \
        (*(h)->freefun)((h)->extra_arg, (old_chunk));                         \
      else                                                                    \
        (*(void (*)(void *))(h)->freefun)((old_chunk));                       \
    } while (0)

/* Initialize an obstack H for use.  Specify chunk size SIZE (0 means default).
   Objects start on multiples of ALIGNMENT (0 means use default).
   CHUNKFUN is the function to use to allocate chunks,
   and FREEFUN the function to free them.

   Return nonzero if successful, calls obstack_alloc_failed_handler if
   allocation fails.  */

int
_obstack_begin (struct obstack *h,
                int size, int alignment,
                void *(*chunkfun) (long),
                void (*freefun) (void *))
{
  struct _obstack_chunk *chunk; /* points to new chunk */

  if (alignment == 0)
    alignment = DEFAULT_ALIGNMENT;
  if (size == 0)
    /* Default size is what GNU malloc can fit in a 4096-byte block.  */
    {
      /* 12 is sizeof (mhead) and 4 is EXTRA from GNU malloc.
         Use the values for range checking, because if range checking is off,
         the extra bytes won't be missed terribly, but if range checking is on
         and we used a larger request, a whole extra 4096 bytes would be
         allocated.

         These number are irrelevant to the new GNU malloc.  I suspect it is
         less sensitive to the size of the request.  */
      int extra = ((((12 + DEFAULT_ROUNDING - 1) & ~(DEFAULT_ROUNDING - 1))
                    + 4 + DEFAULT_ROUNDING - 1)
                   & ~(DEFAULT_ROUNDING - 1));
      size = 4096 - extra;
    }

  h->chunkfun = (struct _obstack_chunk * (*) (void *, long)) chunkfun;
  h->freefun = (void (*) (void *, struct _obstack_chunk *)) freefun;
  h->chunk_size = size;
  h->alignment_mask = alignment - 1;
  h->use_extra_arg = 0;

  chunk = h->chunk = CALL_CHUNKFUN (h, h->chunk_size);
  if (!chunk)
    (*obstack_alloc_failed_handler) ();
  h->next_free = h->object_base = __PTR_ALIGN ((char *) chunk, chunk->contents,
                                               alignment - 1);
  h->chunk_limit = chunk->limit
    = (char *) chunk + h->chunk_size;
  chunk->prev = 0;
  /* The initial chunk now contains no empty object.  */
  h->maybe_empty_object = 0;
  h->alloc_failed = 0;
  return 1;
}

#else
#include <obstack.h>
#endif

#include "wrappedlibs.h"

#include "box64stack.h"
#include "x64emu.h"
#include "debug.h"
#include "wrapper.h"
#include "bridge.h"
#include "callback.h"
#include "librarian.h"
#include "librarian/library_private.h"
#include "emu/x64emu_private.h"
#include "box64context.h"
#include "myalign.h"
#include "signals.h"
#include "fileutils.h"
#include "auxval.h"
#include "elfloader.h"
#include "bridge.h"

typedef void    (*vFv_t)    ();
typedef int32_t (*iFppp_t)  (void*, void*, void*);
typedef int32_t (*iFpLLpp_t)(void*, size_t, size_t, void*, void*);

// utility functions
#define SUPER() \
GO(0)   \
GO(1)   \
GO(2)   \
GO(3)   \
GO(4)

// chunkfun
#define GO(A)   \
static uintptr_t my_chunkfun_fct_##A = 0;                                      \
static void* my_chunkfun_##A(size_t a) \
{                                                                               \
    return (void*)RunFunction(my_context, my_chunkfun_fct_##A, 1, a);            \
}
SUPER()
#undef GO
static void* findchunkfunFct(void* fct)
{
    if(!fct) return NULL;
    void* p;
    if((p = GetNativeFnc((uintptr_t)fct))) return p;
    #define GO(A) if(my_chunkfun_fct_##A == (uintptr_t)fct) return my_chunkfun_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_chunkfun_fct_##A == 0) {my_chunkfun_fct_##A = (uintptr_t)fct; return my_chunkfun_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libc chunkfun callback\n");
    return NULL;
}
static void* reverse_chunkfunFct(library_t* lib, void* fct)
{
    if(!fct) return fct;
    if(CheckBridged(lib->priv.w.bridge, fct))
        return (void*)CheckBridged(lib->priv.w.bridge, fct);
    #define GO(A) if(my_chunkfun_##A == fct) return (void*)my_chunkfun_fct_##A;
    SUPER()
    #undef GO
    return (void*)AddBridge(lib->priv.w.bridge, pFL, fct, 0, NULL);
}
// freefun
#define GO(A)   \
static uintptr_t my_freefun_fct_##A = 0;                \
static void my_freefun_##A(void* a)                     \
{                                                       \
    RunFunction(my_context, my_freefun_fct_##A, 1, a);  \
}
SUPER()
#undef GO
static void* findfreefunFct(void* fct)
{
    if(!fct) return NULL;
    void* p;
    if((p = GetNativeFnc((uintptr_t)fct))) return p;
    #define GO(A) if(my_freefun_fct_##A == (uintptr_t)fct) return my_freefun_##A;
    SUPER()
    #undef GO
    #define GO(A) if(my_freefun_fct_##A == 0) {my_freefun_fct_##A = (uintptr_t)fct; return my_freefun_##A; }
    SUPER()
    #undef GO
    printf_log(LOG_NONE, "Warning, no more slot for libc freefun callback\n");
    return NULL;
}
static void* reverse_freefunFct(library_t* lib, void* fct)
{
    if(!fct) return fct;
    if(CheckBridged(lib->priv.w.bridge, fct))
        return (void*)CheckBridged(lib->priv.w.bridge, fct);
    #define GO(A) if(my_freefun_##A == fct) return (void*)my_freefun_fct_##A;
    SUPER()
    #undef GO
    return (void*)AddBridge(lib->priv.w.bridge, vFp, fct, 0, NULL);
}

#undef SUPER

struct i386_obstack
{
  long	chunk_size;
  struct _obstack_chunk *chunk;
  char	*object_base;
  char	*next_free;
  char	*chunk_limit;
  union
  {
    uintptr_t tempint;
    void *tempptr;
  } temp;
  int   alignment_mask;
  struct _obstack_chunk *(*chunkfun) (void *, long);
  void (*freefun) (void *, struct _obstack_chunk *);
  void *extra_arg;
  unsigned use_extra_arg:1;
  unsigned maybe_empty_object:1;
  unsigned alloc_failed:1;
} __attribute__((packed));

#define SUPER() \
GO(chunk_size)  \
GO(chunk)       \
GO(object_base) \
GO(next_free)   \
GO(chunk_limit) \
GO(temp.tempint)\
GO(alignment_mask)      \
GO(extra_arg)           \
GO(use_extra_arg)       \
GO(maybe_empty_object)  \
GO(alloc_failed)

void to_i386_obstack(struct i386_obstack *_i386, struct obstack *native)
{
    #define GO(A)   _i386->A = native->A;
    SUPER();
    #undef GO
    _i386->chunkfun = findchunkfunFct(native->chunkfun);
    _i386->freefun = findfreefunFct(native->freefun);
}

void from_i386_obstack(struct i386_obstack *_i386, struct obstack *native)
{
    #define GO(A)   native->A = _i386->A;
    SUPER();
    #undef GO
    native->chunkfun = reverse_chunkfunFct(my_context->libclib, _i386->chunkfun);
    native->freefun = reverse_freefunFct(my_context->libclib, _i386->freefun);
}
#undef SUPER

EXPORT int my__obstack_begin(struct i386_obstack * obstack, size_t size, size_t alignment, void* chunkfun, void* freefun)
{
    struct obstack native = {0};
    from_i386_obstack(obstack, &native);    // is this needed?
    int ret = _obstack_begin(&native, size, alignment, findchunkfunFct(chunkfun), findfreefunFct(freefun));
    to_i386_obstack(obstack, &native);
    return ret;
}

EXPORT void my_obstack_free(struct i386_obstack * obstack, void* block)
{
    struct obstack native = {0};
    from_i386_obstack(obstack, &native);
    obstack_free(&native, block);
    to_i386_obstack(obstack, &native);  // usefull??
}
EXPORT void my__obstack_free(struct i386_obstack * obstack, void* block) __attribute__((alias("my_obstack_free")));

EXPORT void my__obstack_newchunk(x64emu_t* emu, struct i386_obstack* obstack, int s)
{
    struct obstack native = {0};
    from_i386_obstack(obstack, &native);
    _obstack_newchunk(&native, s);
    to_i386_obstack(obstack, &native);  //usefull??
}

EXPORT int32_t my_obstack_vprintf(x64emu_t* emu, struct i386_obstack* obstack, void* fmt, x64_va_list_t V)
{
    struct obstack native = {0};
    from_i386_obstack(obstack, &native);
    #ifdef CONVERT_VALIST
    CONVERT_VALIST(V);
    #else
    myStackAlignValist(emu, (const char*)fmt, emu->scratch, V);
    PREPARE_VALIST;
    #endif
    int r = obstack_vprintf(&native, (const char*)fmt, VARARGS);
    to_i386_obstack(obstack, &native);  //usefull??
    return r;
}

EXPORT void* my_obstack_alloc_failed_handler = NULL;
void* ref_obstack_alloc_failed_handler = NULL;
vFv_t real_obstack_alloc_failed_handler = NULL;
void actual_obstack_alloc_failed_handler()
{
    if(ref_obstack_alloc_failed_handler == my_obstack_alloc_failed_handler)
        real_obstack_alloc_failed_handler();
    RunFunction(my_context, (uintptr_t)my_obstack_alloc_failed_handler, 0);
}
void obstackSetup()
{
    // save the real function
    real_obstack_alloc_failed_handler = obstack_alloc_failed_handler;
    // bridge the real function for x64 world
    my_obstack_alloc_failed_handler = (void*)AddCheckBridge(my_context->system, vFv, real_obstack_alloc_failed_handler, 0, NULL);
    ref_obstack_alloc_failed_handler = my_obstack_alloc_failed_handler;
    // setup our version of the function
    obstack_alloc_failed_handler = actual_obstack_alloc_failed_handler;

}
