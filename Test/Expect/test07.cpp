#include <propeller.h>
#include "test07.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif

int32_t test07::Donext(void)
{
  int32_t result = 0;
  Count = (Count + 1);
  return Count;
}

