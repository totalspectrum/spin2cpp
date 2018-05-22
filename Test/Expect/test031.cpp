#include <propeller.h>
#include "test031.h"

void test031::Fun(int32_t Y)
{
  int32_t 	_tmp__0001;
  switch((_tmp__0001 = X + Y)) {
  case 10:
    OUTA ^= 0x1;
    break;
  case (A * 2):
    OUTA ^= 0x2;
    OUTA ^= 0x4;
    break;
  default:
    OUTA ^= 0x8;
    break;
  }
  X = X + 5;
}

