#include <propeller.h>
#include "test47.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

int32_t test47::Test(int32_t C)
{
  int32_t result = 0;
  if (Flag == 0) {
    if (C == 9) {
      do {
        OUTA = (Cols++);
      } while (Cols & 0x7);
    } else if (C == 13) {
      OUTA = C;
    } else if (1) {
      OUTA = Flag;
    }
  } else if (Flag == 10) {
    Cols = (C % Cols);
  }
  Flag = 0;
  return result;
}

