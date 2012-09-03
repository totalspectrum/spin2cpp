#include <propeller.h>
#include "test29.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define Yield__() __asm__ volatile( "" ::: "memory" )
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#define Yield__()
#endif

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

