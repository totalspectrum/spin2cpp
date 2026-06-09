#define __SPIN2CPP__
#include <propeller.h>
#include "test142.h"

#define INLINE__ static inline
INLINE__ int32_t Shr__(uint32_t a, uint32_t b) { return (a>>b); }
uint32_t test142::Hibyte(uint32_t X)
{
  return ((Shr__(X, 24)) & 0xff);
}

