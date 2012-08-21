#include <propeller.h>
#include "test27.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif

int32_t test27::Blink(void)
{
  int32_t result = 0;
  X = (-X);
  return result;
}

