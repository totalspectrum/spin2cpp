#include <propeller.h>
#include "test52.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif

int32_t test52::Func(void)
{
  int32_t result = 0;
  return ((_INA >> 1) & 0x3);
}

