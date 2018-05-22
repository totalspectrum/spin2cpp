#include <propeller.h>
#include "test063.h"

void test063::Test(int32_t Exponent)
{
  switch(Exponent) {
  case 0:
    (++Exponent);
    break;
  default:
    (--Exponent);
    break;
  }
}

