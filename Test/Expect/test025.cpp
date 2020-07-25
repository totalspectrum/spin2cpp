#include <propeller.h>
#include "test025.h"

int32_t test025::Unlock(void)
{
  int32_t 	_tmp__0000;
  if (X != 0) {
    _tmp__0000 = X;
    X = -1;
    return _tmp__0000;
  }
  return 0;
}

