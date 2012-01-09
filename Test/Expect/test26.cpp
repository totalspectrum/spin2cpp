#include <sys/thread.h>
#include <propeller.h>
#include "test26.h"

#define Yield__() (__napuntil(_CNT + 256))
int32_t test26::lock(void)
{
  int32_t result = 0;
  while (lockset(thelock) != 0) {
    Yield__();
  }
  x = 0;
  while (x < 4) {
    lockret(x);
    (x++);
  }
  return result;
}

