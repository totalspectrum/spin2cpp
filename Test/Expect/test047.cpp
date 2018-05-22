#include <propeller.h>
#include "test047.h"

void test047::Test(int32_t C)
{
  switch(Flag) {
  case 0:
    switch(C) {
    case 9:
      do {
        OUTA = Cols++;
      } while (Cols & 0x7);
      break;
    case 13:
      OUTA = C;
      break;
    default:
      OUTA = Flag;
      break;
    }
    break;
  case 10:
    Cols = C % Cols;
    break;
  }
  Flag = 0;
}

