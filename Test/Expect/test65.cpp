#include <propeller.h>
#include "test65.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define Yield__() __asm__ volatile( "" ::: "memory" )
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
INLINE__ int32_t PostFunc__(int32_t *x, int32_t y) { int32_t t = *x; *x = y; return t; }
#define PostEffect__(X, Y) PostFunc__(&(X), (Y))
#define Yield__()
#endif

int32_t test65::Dofunc(int32_t X)
{
  int32_t result = 0;
  do {
    Yield__();
  } while (X);
  return result;
}

