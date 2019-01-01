#include <propeller.h>
#include "test032.h"

void test032::Fun(int32_t Y)
{
  int32_t 	_tmp__0001;
  _tmp__0001 = X + Y;
   if ((_tmp__0001 == 10) || (_tmp__0001 == 'C')) {
    _OUTA ^= 0x1;
  } else if (_tmp__0001 == (A * 2)) {
    _OUTA ^= 0x2;
  } else if ((30 <= _tmp__0001) && (_tmp__0001 <= 40)) {
    _OUTA ^= 0x4;
    _OUTA ^= 0x8;
  } else if (1) {
    _OUTA ^= 0x10;
  }
  X = X + 5;
}

