#include <propeller.h>
#include "test129.h"

void test129::Blink(int32_t Pin, int32_t N)
{
  int32_t	_idx__0000, _limit__0001;
  for(( (_idx__0000 = 0), (_limit__0001 = N) ); _idx__0000 < _limit__0001; _idx__0000++) {
    OUTA ^= (1 << Pin);
  }
}

