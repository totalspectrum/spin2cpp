#include <propeller.h>
#include "test052.h"

#define INLINE__ static inline
INLINE__ int32_t Shr__(uint32_t a, uint32_t b) { return (a>>b); }
int32_t test052::Func(void)
{
  return ((Shr__(_INA, 1)) & 0x3);
}

