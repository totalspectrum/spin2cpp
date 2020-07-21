#include <propeller.h>
#include "test032.h"

void test032::Fun(int32_t Y)
{
  int32_t 	_tmp__0000;
  _tmp__0000 = X + Y;
  if ((_tmp__0000 == 10) || (_tmp__0000 == 67)) {
    goto _case__0002;
  }
  if (_tmp__0000 == (A * 2)) {
    goto _case__0003;
  }
  if ((_tmp__0000 >= 30) && (_tmp__0000 <= 40)) {
    goto _case__0004;
  }
  goto _case__0005;
  _case__0002: ;
  _OUTA ^= 0x1;
  goto _endswitch_0001;
  _case__0003: ;
  _OUTA ^= 0x2;
  goto _endswitch_0001;
  _case__0004: ;
  _OUTA ^= 0x4;
  _OUTA ^= 0x8;
  goto _endswitch_0001;
  _case__0005: ;
  _OUTA ^= 0x10;
  goto _endswitch_0001;
  _endswitch_0001: ;
  X = X + 5;
}

