#include <propeller.h>
#include "test84.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif

int32_t test84::Getcogid(void)
{
  int32_t result = 0;
  return (Cog - 1);
}

