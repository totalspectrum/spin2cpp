#ifndef _SETJMP_H
#define _SETJMP_H

#include <sys/jmpbuf.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef _jmp_buf jmp_buf;

#ifdef __FLEXC__
#define longjmp(env, val) __builtin_longjmp(&(env)[0], (val))
#define setjmp(env) __builtin_setjmp(&(env)[0])
#else    
void longjmp(jmp_buf env, int val);
int  setjmp(jmp_buf env);
#endif
    
#if defined(__cplusplus)
}
#endif

#endif
