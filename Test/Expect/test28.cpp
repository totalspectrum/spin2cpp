#include <propeller.h>
#include "test28.h"

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

