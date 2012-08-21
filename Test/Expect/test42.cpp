#include <propeller.h>
#include "test42.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif

int32_t test42::Getx(void)
{
  int32_t result = 0;
  return A.X;
}

