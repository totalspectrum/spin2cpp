#include <propeller.h>
#include "test08.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif

int32_t test08::Init(void)
{
  int32_t result = 0;
  _OUTA = 1;
  return result;
}

