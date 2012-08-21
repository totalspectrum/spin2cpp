#include <propeller.h>
#include "test57.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif

uint8_t test57::dat[] = {
  0x00, 0x00, 0x80, 0x40, 
};
int32_t test57::Getval(void)
{
  int32_t result = 0;
  return (*(int32_t *)&dat[0]);
}

