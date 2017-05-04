#include <stdlib.h>
#include <propeller.h>
#include "test129.h"

void test129::Blink(int32_t Pin, int32_t N)
{
  int32_t 	_idx__0001, _limit__0002;
  for(( (_idx__0001 = 0), (_limit__0002 = N) ); _idx__0001 < _limit__0002; _idx__0001++) {
    OUTA ^= (1 << Pin);
  }
}

