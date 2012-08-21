#include <propeller.h>
#include "test46.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif

INLINE__ int32_t Between__(int32_t x, int32_t a, int32_t b){ if (a <= b) return x >= a && x <= b; return x >= b && x <= a; }

int32_t test46::Fun(int32_t X, int32_t Y)
{
  int32_t result = 0;
  if (X == 0) {
    if (Y == 0) {
      _OUTA ^= 0x1;
    } else if (Y == 1) {
      _OUTA ^= 0x2;
    }
  } else if (Between__(X, 10, 12)) {
    _OUTA ^= 0x4;
  } else if (1) {
    _OUTA ^= 0x8;
  }
  return result;
}

