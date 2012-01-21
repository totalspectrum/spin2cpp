#include <propeller.h>
#include "test69.h"

int32_t test69::Demo(void)
{
  int32_t result = 0;
  if (!(((_INA >> 0) & 0x1))) {
    abort();
  } else {
    if (!(((_INA >> 1) & 0x1))) {
      _OUTA = (_OUTA & 0xfffffff8) | ((0 << 0) & 0x00000007);
    }
  }
  return result;
}

