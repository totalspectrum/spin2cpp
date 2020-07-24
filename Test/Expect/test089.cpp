#include <propeller.h>
#include "test089.h"

void test089::Catchlong(int32_t Longvar)
{
  B0 = (Longvar >> 0) & 0xff;
  B1 = (Longvar >> 8) & 0xff;
  B2 = (Longvar >> 16) & 0xff;
  B3 = (Longvar >> 24) & 0xff;
}

