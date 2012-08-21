#include <propeller.h>
#include "test12.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif
INLINE__ int32_t PostFunc__(int32_t *x, int32_t y) { int32_t t = *x; *x = y; return t; }
#define PostEffect__(X, Y) PostFunc__(&(X), (Y))

int32_t test12::Sum(int32_t X, int32_t Y)
{
  int32_t result = 0;
  return (X + Y);
}

int32_t test12::Donext(int32_t X)
{
  int32_t result = 0;
  return Sum(X, 1);
}

