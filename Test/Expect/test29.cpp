#include <propeller.h>
#include "test29.h"

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

int32_t test29::Tx(int32_t Val)
{
  int32_t result = 0;
  ((uint8_t *)28672)[Idx] = 0;
  return result;
}

int32_t test29::Str(int32_t Stringptr)
{
  int32_t result = 0;
  while (lockset(Strlock)) {
    Yield__();
  }
  {
    int32_t _idx__0000;
    _idx__0000 = strlen((char *) Stringptr);
    do {
      Tx(((uint8_t *)(Stringptr++))[0]);
      _idx__0000 = (_idx__0000 + -1);
    } while (_idx__0000 >= 1);
  }
  lockclr(Strlock);
  return result;
}

