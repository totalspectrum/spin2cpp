#include <propeller.h>
#include "test47.h"

int32_t test47::test(int32_t c)
{
  int32_t result = 0;
  if (flag == 0) {
    if (c == 9) {
      do {
        _OUTA = (cols++);
      } while (cols & 7);
    } else if (c == 13) {
      _OUTA = c;
    } else if (1) {
      _OUTA = flag;
    }
  } else if (flag == 10) {
    cols = (c % cols);
  }
  flag = 0;
  return result;
}

