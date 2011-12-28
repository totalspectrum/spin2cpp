#include <propeller.h>
#include "test25.h"

int32_t test25::unlock(void)
{
  int32_t result = 0;
  if ((x != 0)) {
    return __extension__({ int32_t _tmp_ = x; x = 0; _tmp_; });
  }
}

