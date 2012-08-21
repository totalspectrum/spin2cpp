#include <propeller.h>
#include "test58.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif

uint8_t test58::dat[] = {
  0xdb, 0x0f, 0xc9, 0x3f, 
};
int32_t test58::Getval(void)
{
  int32_t result = 0;
  return (*(int32_t *)&dat[0]);
}

