#include <propeller.h>
#include "test23.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif

int32_t test23::Start(void)
{
  int32_t	X;
  int32_t R = 0;
  X = locknew();
  R = lockclr(X);
  return R;
}

