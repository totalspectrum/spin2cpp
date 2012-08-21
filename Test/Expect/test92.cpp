#include <propeller.h>
#include "test92.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif

int32_t test92::Inc(void)
{
  int32_t result = 0;
  X = (X + 1);
  return result;
}

