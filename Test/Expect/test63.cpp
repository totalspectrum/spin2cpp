#include <propeller.h>
#include "test63.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif

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

