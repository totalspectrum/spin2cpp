#include <propeller.h>
#include "test31.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif

int32_t test31::Fun(int32_t Y)
{
  int32_t result = 0;
  int32_t _tmp__0000 = (X + Y);
  if (_tmp__0000 == 10) {
    _OUTA ^= 0x1;
  } else if (_tmp__0000 == (4 * 2)) {
    _OUTA ^= 0x2;
    _OUTA ^= 0x4;
  } else if (1) {
    _OUTA ^= 0x8;
  }
  X = (X + 5);
  return result;
}

