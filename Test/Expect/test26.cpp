#include <propeller.h>
#include "test26.h"

int32_t test26::lock(void)
{
  int32_t result = 0;
  while ((lockset(thelock) != 0)) {
  }
  x = 0;
  while ((x < 4)) {
    lockret(x);
    (x++);
  }
  return result;
}

