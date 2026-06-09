#define __SPIN2CPP__
#include <propeller.h>
#include "test124.h"

#define INLINE__ static inline
INLINE__ int32_t Shr__(uint32_t a, uint32_t b) { return (a>>b); }
uint32_t test124::Getval(void)
{
  return ((Shr__((__builtin_propeller_rev(_OUTA, 0)), 28)) & 0x3);
}

