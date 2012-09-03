#include <propeller.h>
#include "test91.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

__NATIVE
int32_t test91::Nativefunc(int32_t I)
{
  int32_t result = 0;
  return (I + 1);
}

