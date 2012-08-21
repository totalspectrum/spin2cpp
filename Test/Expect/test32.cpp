#include <propeller.h>
#include "test32.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif
INLINE__ int32_t PostFunc__(int32_t *x, int32_t y) { int32_t t = *x; *x = y; return t; }
#define PostEffect__(X, Y) PostFunc__(&(X), (Y))

INLINE__ int32_t Between__(int32_t x, int32_t a, int32_t b){ if (a <= b) return x >= a && x <= b; return x >= b && x <= a; }

int32_t test32::Fun(int32_t Y)
{
  int32_t result = 0;
  int32_t _tmp__0000 = (X + Y);
  if (_tmp__0000 == 10 || _tmp__0000 == 'C') {
    _OUTA ^= 0x1;
  } else if (_tmp__0000 == (4 * 2)) {
    _OUTA ^= 0x2;
  } else if (Between__(_tmp__0000, 30, 40)) {
    _OUTA ^= 0x4;
    _OUTA ^= 0x8;
  } else if (1) {
    _OUTA ^= 0x10;
  }
  X = (X + 5);
  return result;
}

