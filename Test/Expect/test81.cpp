#include <propeller.h>
#include "test81.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

int32_t test81::High8(void)
{
  int32_t result = 0;
  OUTA |= (1<<8);
  DIRA |= (1<<8);
  return result;
}

int32_t test81::High(int32_t Pin)
{
  int32_t result = 0;
  OUTA |= (1<<Pin);
  DIRA |= (1<<Pin);
  return result;
}

