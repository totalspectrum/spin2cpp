#include <propeller.h>
#include "test22.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif

int32_t test22::Calc(int32_t X, int32_t Y)
{
  int32_t R = 0;
  R = (X * Y);
  return R;
}

