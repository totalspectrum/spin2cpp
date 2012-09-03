#include <propeller.h>
#include "test28.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

int32_t test28::Lock(void)
{
  int32_t result = 0;
  X = 0;
  while (!(X > 9)) {
    lockret(X);
    (X++);
  }
  X = 0;
  do {
    lockret(X);
    (X++);
  } while (X < 10);
  return result;
}

