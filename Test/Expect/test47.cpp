#include <propeller.h>
#include "test47.h"

int32_t test47::test(int32_t c)
{
  int32_t result = 0;
  switch (flag) {
    case 0:
        switch (c) {
          case 9:
            do {
              _OUTA = (cols++);
            } while (cols & 7);
            break;
          case 13:
            _OUTA = c;
            break;
          default:
            _OUTA = flag;
            break;
        }
      break;
    case 10:
      cols = (c % cols);
      break;
  }
  flag = 0;
  return result;
}

