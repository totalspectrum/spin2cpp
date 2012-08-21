#include <sys/thread.h>
#include <propeller.h>
#include "test26.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif

#define Yield__() (__napuntil(_CNT))
int32_t test26::Lock(void)
{
  int32_t result = 0;
  while (lockset(Thelock) != 0) {
    Yield__();
  }
  X = 0;
  while (X < 4) {
    lockret(X);
    (X++);
  }
  return result;
}

