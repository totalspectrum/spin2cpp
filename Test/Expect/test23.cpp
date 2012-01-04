#include <propeller.h>
#include "test23.h"

int32_t test23::start(void)
{
  int32_t r = 0;
  int32_t	x;
  x = locknew();
  r = lockclr(x);
  return r;
}

