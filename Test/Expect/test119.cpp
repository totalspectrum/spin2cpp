#include <propeller.h>
#include "test119.h"

void test119::Clear(void)
{
  int32_t	I;
  for(I = 0; I < Width; I++) {
    Array[I] = 0;
  }
}

