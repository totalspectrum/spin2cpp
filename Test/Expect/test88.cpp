#include <propeller.h>
#include "test88.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif

int32_t test88::Sum(void)
{
  int32_t	R, X;
  int32_t result = 0;
  R = 0;
  X = 0;
  do {
    R = (R + X);
    X = (X + 1);
  } while (X <= 4);
  return R;
}

