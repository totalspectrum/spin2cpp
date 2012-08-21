#include <propeller.h>
#include "test14.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif

uint8_t test14::dat[] = {
  0x01, 0x00, 0x00, 0x00, 0xdd, 0xcc, 0xbb, 0xaa, 
};
int32_t test14::Myfunc(void)
{
  int32_t result = 0;
  return (*(int32_t *)&dat[4]);
}

