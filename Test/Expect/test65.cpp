#include <propeller.h>
#include "test65.h"

#ifdef __GNUC__
#define INLINE__ static inline
#include <sys/thread.h>
#define Yield__() (__napuntil(_CNT))
#else
#define INLINE__ static
#define Yield__()
#endif
INLINE__ int32_t PostFunc__(int32_t *x, int32_t y) { int32_t t = *x; *x = y; return t; }
#define PostEffect__(X, Y) PostFunc__(&(X), (Y))

int32_t test65::Dofunc(int32_t X)
{
  int32_t result = 0;
  do {
    Yield__();
  } while (X);
  return result;
}

