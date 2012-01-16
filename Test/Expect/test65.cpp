#include <sys/thread.h>
#include <propeller.h>
#include "test65.h"

#define Yield__() (__napuntil(_CNT + 1))
int32_t test65::dofunc(int32_t x)
{
  int32_t result = 0;
  do {
    Yield__();
  } while (x);
  return result;
}

