#include <propeller.h>
#include "test028.h"

void test028::Lock(void)
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

