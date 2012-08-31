#include <propeller.h>
#include "test45.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
INLINE__ int32_t PostFunc__(int32_t *x, int32_t y) { int32_t t = *x; *x = y; return t; }
#define PostEffect__(X, Y) PostFunc__(&(X), (Y))
#endif

int32_t test45::Fun(int32_t X, int32_t Y)
{
  int32_t result = 0;
  if (X == 0) {
    if (Y == 0) {
      OUTA ^= 0x1;
    } else if (Y == 1) {
      OUTA ^= 0x2;
    }
  } else if (X == 20) {
    OUTA ^= 0x4;
  } else if (1) {
    OUTA ^= 0x8;
  }
  return result;
}

