#include <propeller.h>
#include "test87.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif

int32_t test87::Check(int32_t N)
{
  int32_t Sum = 0;
  Sum = 0;
  if (N > 1) {
    while (N--) {
      Sum = (Sum + N);
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

