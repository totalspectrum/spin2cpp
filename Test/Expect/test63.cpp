#include <propeller.h>
#include "test63.h"

void test63::Test(int32_t Exponent)
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

