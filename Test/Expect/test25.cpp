#include <propeller.h>
#include "test25.h"

int32_t test25::Unlock(void)
{
  int32_t	_tmp__0001;
  if (X != 0) {
    return ( ( (_tmp__0001 = X), (X = -1) ), _tmp__0001 );
  }
  return 0;
}

