#include <propeller.h>
#include "btest004.h"

void btest004::copy(void)
{
  int32_t 	i;
  for(i = 1; i < 11; i++) {
    a[(i - 1)] = b[(i - 1)];
  }
}

