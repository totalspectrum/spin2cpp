#include <propeller.h>
#include "test80.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif

int32_t test80::Init(void)
{
  int32_t result = 0;
  Namebuffer[0] = '.';
  return result;
}

