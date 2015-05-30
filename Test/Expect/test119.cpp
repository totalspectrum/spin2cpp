#include <propeller.h>
#include "test119.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

int32_t test119::Clear(void)
{
  int32_t	I;
  for(I = 0; I <= (Width - 1); I = I + 1) {
    Array[I] = 0;
  }
  return 0;
}

