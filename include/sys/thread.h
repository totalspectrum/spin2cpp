/*
 * Implementation of simple threading support
 * Copyright (c) 2011 Parallax, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * +--------------------------------------------------------------------
 */
#ifndef _SYS_THREAD_H
#define _SYS_THREAD_H

#include <sys/jmpbuf.h>
#include <sys/fenv.h>
#include <sys/wchar_t.h>

#if defined(__cplusplus)
extern "C" {
#endif

#ifndef _STRUCT_TM_DEFINED
#define _STRUCT_TM_DEFINED
/* time representing broken down calendar time */
struct tm {
  int tm_sec;
  int tm_min;
  int tm_hour;
  int tm_mday;
  int tm_mon;
  int tm_year; /* years since 1900 */
  int tm_wday; /* days since Sunday */
  int tm_yday; /* days since January 1 */
  int tm_isdst; /* if > 0, DST is in effect, if < 0 info is not known */
};
#endif

/*
 * thread local storage
 * the library should not keep anything in global or
 * static variables; all such values should be placed
 * in the _thread structure
 */
typedef struct _thread _thread_state_t;
typedef _thread_state_t * volatile _thread_queue_t;

struct _thread {
  /* threads may sleep on queues */
  _thread_state_t *queue_next;
  _thread_queue_t *queue;

  int errno;
  char *strtok_scanpoint;

  unsigned long rand_seed;
  struct tm time_temp;
  char ctime_buf[32];
  _jmp_buf jmpbuf;       /* for thread context */
  short pri;             /* thread priority */
  unsigned short flags;  /* flags for this thread */

  unsigned int timer;    /* used for sleeping */
  /* re-use arg for the thread return value */
  void *arg;             /* thread argument */
  void *(*start)(void *);/* start function */

  /* cog affinity mask: note that this is actually the inverse of
     the affinity mask, so if it's 0 (the default) we can run on any
     cog
  */
  unsigned short affinity; /* processor affinity mask inverted */

  /* multibyte character conversion state */
  _Mbstate_t mbr_intern;

  /* floating point environment; not used currently */
  /* _fenv_t fenv; */

  /* thread specific data for users to store */
#define _THREAD_KEYS_MAX 4
  void *thread_data[_THREAD_KEYS_MAX];
};

/*
 * we may want to define thread local storage in some special way
 * to ensure it is preserved in task switches
 */
#if defined(__propeller__) && defined(__GNUC__)
#define _TLSDECL(x) extern __attribute__((cogmem)) x
#else
#define _TLSDECL(x) extern x
#endif

_TLSDECL( _thread_state_t *_TLS );

/*
 * start a new cog thread running C code
 * (as if "func(arg)" was called in that thread)
 * tls is a pointer to the thread local storage area to use
 */
int _start_cog_thread(void *stacktop, void (*func)(void *), void *arg, _thread_state_t *tls);

/*
 * function to do a "compare and swap" operation on a memory location
 * this is protected with the _C_LOCK lock, so it should be atomic
 * (as long as all threads use this function to access the location)
 * if *ptr == checkval, then set *ptr to newval; returns original
 * value of *ptr
 */
#if !defined(__GNUC__)
#define __sync_bool_compare_and_swap(ptr, oldval, newval) \
  ((*ptr == oldval) && ((*ptr = newval),true)))
#define __sync_add_and_fetch(ptr, inc) \
  (*ptr += inc)
#endif

  /* functions to atomicly fetch or set a 64 bit quantity in HUB memory */
  /* by "atomic" we mean in 2 consecutive hub operations, low then high;
     all users must follow this protocol
  */
  unsigned long long _getAtomic64(unsigned long long *ptr);
  void _putAtomic64(unsigned long long x, unsigned long long *ptr);

/* type for a volatile lock */
/* if we change this type, change the definitions of SIG_ATOMIC_{MIN,MAX}
 * in stdint.h too
 */
/* also note that in practice these have to go in HUB memory */
typedef volatile int _atomic_t;
typedef _atomic_t atomic_t;

#if !defined(__cplusplus)
  /* the GNU C++ library already has locks in it, so don't conflict */

  /* we don't have the necessary primitives in the XMM kernel */
#define __trylock(ptr) __sync_bool_compare_and_swap(ptr, 0, 1)
#define __addlock(ptr, inc) __sync_add_and_fetch(ptr, inc)

#if defined(__GNUC__)
#define __lock(val) while (__builtin_expect(!__trylock(val), 0)) ;
#else
#define __lock(val) while (!__trylock(val)) ;
#endif
#define __unlock(val) *val = 0

#endif /* !defined(__cplusplus) */

/* hook for giving up CPU time while waiting */
extern void (*__yield_ptr)(void);

/* hook for sleeping until the clock() reaches a specific count */
extern void (*__napuntil_ptr)(unsigned int newclock);
extern void __napuntil(unsigned int newclock);

/* macro for finding our own CPU id in a bit mask */
#define __this_cpu_mask() (1<<__builtin_propeller_cogid())

#if defined(__cplusplus)
}
#endif

#endif
