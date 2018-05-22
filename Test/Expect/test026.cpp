#include <propeller.h>
#include "test026.h"

#define Yield__() __asm__ volatile( "" ::: "memory" )
void test026::Lock(void)
{
  while (lockset(Thelock) != 0) {
    Yield__();
  }
  X = 0;
  while (X < 4) {
    lockret(X);
    (X++);
  }
}

