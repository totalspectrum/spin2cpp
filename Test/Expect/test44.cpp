#include <propeller.h>
#include "test44.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif
INLINE__ int32_t PostFunc__(int32_t *x, int32_t y) { int32_t t = *x; *x = y; return t; }
#define PostEffect__(X, Y) PostFunc__(&(X), (Y))

int32_t test44::Fun(int32_t X, int32_t Y)
{
  int32_t result = 0;
  if (X == 10) {
    if (Y == 0) {
      _OUTA ^= 0x1;
    } else if (Y == 1) {
      _OUTA ^= 0x2;
    }
  } else if (X == 20) {
    _OUTA ^= 0x4;
  } else if (1) {
    _OUTA ^= 0x8;
  }
  return result;
}

