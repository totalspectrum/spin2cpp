#include <propeller.h>
#include "test67.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif

uint8_t test67::dat[] = {
  0x3b, 0xaa, 0xb8, 0x3f, 
};
int32_t test67::Getx(void)
{
  int32_t result = 0;
  return (*(int32_t *)&dat[0]);
}

