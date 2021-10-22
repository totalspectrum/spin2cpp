#define __SPIN2CPP__
#include <propeller.h>
#include "test124.h"

#define INLINE__ static inline
INLINE__ int32_t Shr__(uint32_t a, uint32_t b) { return (a>>b); }
int32_t test124::Getval(void)
{
  return (__builtin_propeller_rev(((Shr__(_OUTA, 2)) & 0x3), 32 - 2));
}

