// check that return value processing goes through
// case statements
#include <propeller.h>
#include "test127.h"

int32_t test127::Asc2val(int32_t P_str)
{
  int32_t	C;
  while (1) {
    C = ((uint8_t *)P_str)[0];
    if (C == ' ') {
      (P_str++);
    } else if (('0' <= C) && (C <= '9')) {
      return (C - '0');
    } else if (1) {
      return 0;
    }
  }
  return 0;
}

