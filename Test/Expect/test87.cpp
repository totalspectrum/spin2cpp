#include <propeller.h>
#include "test87.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
INLINE__ int32_t PostFunc__(int32_t *x, int32_t y) { int32_t t = *x; *x = y; return t; }
#define PostEffect__(X, Y) PostFunc__(&(X), (Y))
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

