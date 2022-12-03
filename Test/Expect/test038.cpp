#define __SPIN2CPP__
#include <propeller.h>
#include "test038.h"

#define INLINE__ static inline
INLINE__ int32_t Min__(int32_t a, int32_t b) { return a < b ? a : b; }
INLINE__ int32_t Max__(int32_t a, int32_t b) { return a > b ? a : b; }
int32_t test038::Big(int32_t X, int32_t Y)
{
  int32_t result;
  result = Max__(X, Y);
  return result;
}

int32_t test038::Small(int32_t X, int32_t Y)
{
  int32_t result;
  result = Min__(X, Y);
  return result;
}

