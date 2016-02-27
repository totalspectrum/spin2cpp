#include <propeller.h>
#include "test47.h"

void test47::Test(int32_t C)
{
  if (Flag == 0) {
    if (C == 9) {
      do {
        OUTA = Cols++;
      } while (Cols & 0x7);
    } else if (C == 13) {
      OUTA = C;
    } else if (1) {
      OUTA = Flag;
    }
  } else if (Flag == 10) {
    Cols = C % Cols;
  }
  Flag = 0;
}

