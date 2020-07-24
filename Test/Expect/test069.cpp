#include <propeller.h>
#include "test069.h"

#define INLINE__ static inline
INLINE__ int32_t Shr__(uint32_t a, uint32_t b) { return (a>>b); }
void test069::Demo(void)
{
  if (!((Shr__(_INA, 0)) & 0x1)) {
    clkset(0x80, 0);
  } else {
    if (!((Shr__(_INA, 1)) & 0x1)) {
      _OUTA &= (~(7 << 0));
    }
  }
}

