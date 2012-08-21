#include <propeller.h>
#include "test12.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif

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

