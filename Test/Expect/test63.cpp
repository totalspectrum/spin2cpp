#include <propeller.h>
#include "test63.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif
INLINE__ int32_t PostFunc__(int32_t *x, int32_t y) { int32_t t = *x; *x = y; return t; }
#define PostEffect__(X, Y) PostFunc__(&(X), (Y))

int32_t test63::Test(int32_t Exponent)
{
  int32_t result = 0;
  if (Exponent == 0) {
    (++Exponent);
  } else if (1) {
    (--Exponent);
  }
  return result;
}

