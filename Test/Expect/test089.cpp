#include <propeller.h>
#include "test089.h"

#define INLINE__ static inline
INLINE__ int32_t Shr__(uint32_t a, uint32_t b) { return (a>>b); }
void test089::Catchlong(int32_t Longvar)
{
  B0 = Longvar & 0xff;
  B1 = (Shr__(Longvar, 8)) & 0xff;
  B2 = (Shr__(Longvar, 16)) & 0xff;
  B3 = Shr__(Longvar, 24);
}

