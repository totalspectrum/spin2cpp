#include <propeller.h>
#include "test26.h"

#define Yield__() __asm__ volatile( "" ::: "memory" )
void test26::Lock(void)
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

