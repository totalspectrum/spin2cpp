#include <propeller.h>
#include "test26.h"

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

int32_t test26::Lock(void)
{
  int32_t result = 0;
  while (lockset(Thelock) != 0) {
    Yield__();
  }
  X = 0;
  while (X < 4) {
    lockret(X);
    (X++);
  }
  return result;
}

