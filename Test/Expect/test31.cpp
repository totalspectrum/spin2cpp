#include <propeller.h>
#include "test31.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

int32_t test31::Fun(int32_t Y)
{
  int32_t _tmp__0000 = (X + Y);
  if (_tmp__0000 == 10) {
    OUTA ^= 0x1;
  } else if (_tmp__0000 == (A * 2)) {
    OUTA ^= 0x2;
    OUTA ^= 0x4;
  } else if (1) {
    OUTA ^= 0x8;
  }
  X = (X + 5);
  return 0;
}

