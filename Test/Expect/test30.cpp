#include <propeller.h>
#include "test30.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

int32_t test30::Test(void)
{
  int32_t X = 0;
  if ((A == 0) && (B == 0)) {
    return 0;
  } else {
    if (A == 0) {
      return 1;
    } else {
      if (B == 0) {
        return 2;
      } else {
        X = -1;
      }
    }
  }
  return X;
}

