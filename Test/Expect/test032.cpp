#include <propeller.h>
#include "test032.h"

void test032::Fun(int32_t Y)
{
  int32_t 	_tmp__0001;
  _tmp__0001 = X + Y;
  if ((_tmp__0001 == 10) || (_tmp__0001 == 67)) {
    goto _case__0003;
  }
  if (_tmp__0001 == (A * 2)) {
    goto _case__0004;
  }
  if ((_tmp__0001 >= 30) && (_tmp__0001 <= 40)) {
    goto _case__0005;
  }
  goto _case__0006;
  _case__0003: ;
  _OUTA ^= 0x1;
  goto _endswitch_0002;
  _case__0004: ;
  _OUTA ^= 0x2;
  goto _endswitch_0002;
  _case__0005: ;
  _OUTA ^= 0x4;
  _OUTA ^= 0x8;
  goto _endswitch_0002;
  _case__0006: ;
  _OUTA ^= 0x10;
  goto _endswitch_0002;
  _endswitch_0002: ;
  X = X + 5;
}

