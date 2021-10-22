#define __SPIN2CPP__
#include <propeller.h>
#include "btest004.h"

void btest004::copy(void)
{
  int32_t 	i;
  for(i = 1; i < 11; i++) {
    a[(i - 0)] = b[(i - 0)];
  }
}

