#include <propeller.h>
#include "test69.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif

int32_t test69::Demo(void)
{
  int32_t result = 0;
  if (!(((_INA >> 0) & 0x1))) {
    abort();
  } else {
    if (!(((_INA >> 1) & 0x1))) {
      _OUTA &= ~(7<<0);
    }
  }
  return result;
}

