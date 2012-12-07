#include <propeller.h>
#include "test86.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

int32_t test86::Set1(void)
{
  OUTA = ((OUTA & 0xfffffffd) | 0x2);
  return 0;
}

int32_t test86::Set(int32_t Pin)
{
  OUTA = ((OUTA & (~(1 << Pin))) | (1 << Pin));
  return 0;
}

