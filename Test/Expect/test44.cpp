#include <propeller.h>
#include "test44.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

void test44::Fun(int32_t X, int32_t Y)
{
  if (X == 10) {
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
}

