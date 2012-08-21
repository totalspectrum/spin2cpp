#include <propeller.h>
#include "test82.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif
INLINE__ int32_t PostFunc__(int32_t *x, int32_t y) { int32_t t = *x; *x = y; return t; }
#define PostEffect__(X, Y) PostFunc__(&(X), (Y))

int32_t test82::Flip(void)
{
  int32_t result = 0;
  X = (~X);
  return result;
}

int32_t test82::Toggle(int32_t Pin)
{
  int32_t result = 0;
  OUTA ^= (1<<Pin);
  return result;
}

