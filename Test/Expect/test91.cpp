#include <propeller.h>
#include "test91.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif

__NATIVE
int32_t test91::Nativefunc(int32_t I)
{
  int32_t result = 0;
  return (I + 1);
}

