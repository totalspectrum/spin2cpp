#include <propeller.h>
#include "test87.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

int32_t test87::Check(int32_t N)
{
  int32_t Sum = 0;
  Sum = 0;
  if (N > 1) {
    /*  other part  */
    while (N--) {
      Sum = Sum + N;
    }
  } else {
    if (N < (-1)) {
      return (-1);
    } else {
      return 0;
    }
  }
  return Sum;
}

