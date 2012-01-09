#include <propeller.h>
#include "test28.h"

int32_t test28::lock(void)
{
  int32_t result = 0;
  x = 0;
  while (!(x > 9)) {
    lockret(x);
    (x++);
  }
  x = 0;
  do {
    lockret(x);
    (x++);
  } while (x < 10);
  return result;
}

