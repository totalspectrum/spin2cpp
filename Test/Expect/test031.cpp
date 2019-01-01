#include <propeller.h>
#include "test031.h"

void test031::Fun(int32_t Y)
{
  int32_t 	_tmp__0001;
  switch((_tmp__0001 = X + Y)) {
  case 10:
    _OUTA ^= 0x1;
    break;
  case (A * 2):
    _OUTA ^= 0x2;
    _OUTA ^= 0x4;
    break;
  default:
    _OUTA ^= 0x8;
    break;
  }
  X = X + 5;
}

