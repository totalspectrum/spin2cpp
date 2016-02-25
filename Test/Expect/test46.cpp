#include <propeller.h>
#include "test46.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

void test46::Fun(int32_t X, int32_t Y)
{
  if (X == 0) {
    if (Y == 0) {
      OUTA ^= 0x1;
    } else if (Y == 1) {
      OUTA ^= 0x2;
    }
  } else if ((10 <= X) && (X <= 12)) {
    OUTA ^= 0x4;
  } else if (1) {
    OUTA ^= 0x8;
  }
}

