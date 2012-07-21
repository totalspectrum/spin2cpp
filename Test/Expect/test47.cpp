#include <propeller.h>
#include "test47.h"

int32_t test47::Test(int32_t C)
{
  int32_t result = 0;
  if (Flag == 0) {
    if (C == 9) {
      do {
        _OUTA = (Cols++);
      } while (Cols & 0x7);
    } else if (C == 13) {
      _OUTA = C;
    } else if (1) {
      _OUTA = Flag;
    }
  } else if (Flag == 10) {
    Cols = (C % Cols);
  }
  Flag = 0;
  return result;
}

