#include <sys/thread.h>
#include <propeller.h>
#include "test62.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif

#define Yield__() (__napuntil(_CNT))
int32_t test62::Testit(int32_t X)
{
  int32_t result = 0;
  Command = X;
  while (Command) {
    Yield__();
  }
  return Response;
}

