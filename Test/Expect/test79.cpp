#include <propeller.h>
#include "test79.h"

int32_t test79::I2c_start(void)
{
  int32_t result = 0;
  __extension__({ int32_t _tmp_ = ((_OUTA >> 28) & 1); _OUTA = (_OUTA & 0xefffffff) | ((-1 << 28) & 0x10000000); _tmp_; });
  __extension__({ int32_t _tmp_ = ((_DIRA >> 28) & 1); _DIRA = (_DIRA & 0xefffffff) | ((-1 << 28) & 0x10000000); _tmp_; });
  __extension__({ int32_t _tmp_ = ((_OUTA >> 29) & 1); _OUTA = (_OUTA & 0xdfffffff) | ((-1 << 29) & 0x20000000); _tmp_; });
  __extension__({ int32_t _tmp_ = ((_DIRA >> 29) & 1); _DIRA = (_DIRA & 0xdfffffff) | ((-1 << 29) & 0x20000000); _tmp_; });
  __extension__({ int32_t _tmp_ = ((_OUTA >> 29) & 1); _OUTA &= ~(1<<29); _tmp_; });
  __extension__({ int32_t _tmp_ = ((_OUTA >> 28) & 1); _OUTA &= ~(1<<28); _tmp_; });
  return result;
}

