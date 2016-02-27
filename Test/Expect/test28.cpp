#include <propeller.h>
#include "test28.h"

void test28::Lock(void)
{
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
}

