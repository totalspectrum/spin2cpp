#include <propeller.h>
#include "test24.h"

int32_t test24::unlock(void)
{
  int32_t result = 0;
  return __extension__({ int32_t _tmp_ = x; x = 0; _tmp_; });
}

