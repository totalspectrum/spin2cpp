#include <propeller.h>
#include "test025.h"

int32_t test025::Unlock(void)
{
  int32_t 	_tmp__0001;
  if (X != 0) {
    _tmp__0001 = X;
    X = -1;
    return _tmp__0001;
  }
  return 0;
}

