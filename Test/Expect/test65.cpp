#include <sys/thread.h>
#include <propeller.h>
#include "test65.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif

#define Yield__() (__napuntil(_CNT))
int32_t test65::Dofunc(int32_t X)
{
  int32_t result = 0;
  do {
    Yield__();
  } while (X);
  return result;
}

