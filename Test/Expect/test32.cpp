#include <propeller.h>
#include "test32.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

INLINE__ int32_t Between__(int32_t x, int32_t a, int32_t b){ if (a <= b) return x >= a && x <= b; return x >= b && x <= a; }

int32_t test32::Fun(int32_t Y)
{
  int32_t result = 0;
  int32_t _tmp__0000 = (X + Y);
  if (_tmp__0000 == 10 || _tmp__0000 == 'C') {
    OUTA ^= 0x1;
  } else if (_tmp__0000 == (A * 2)) {
    OUTA ^= 0x2;
  } else if (Between__(_tmp__0000, 30, 40)) {
    OUTA ^= 0x4;
    OUTA ^= 0x8;
  } else if (1) {
    OUTA ^= 0x10;
  }
  X = (X + 5);
  return result;
}

