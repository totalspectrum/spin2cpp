#include <propeller.h>
#include "test032.h"

void test032::Fun(int32_t Y)
{
  int32_t 	_tmp__0001, _tmp__0002;
  _tmp__0002 = _tmp__0001 = X + Y;
  if ((_tmp__0002 == 10) || (_tmp__0002 == 67)) {
    goto _case__0004;
  }
  if (_tmp__0002 == (A * 2)) {
    goto _case__0005;
  }
  if ((_tmp__0002 >= 30) && (_tmp__0002 <= 40)) {
    goto _case__0006;
  }
  goto _case__0007;
  _case__0004:
  _OUTA ^= 0x1;
  goto _endswitch_0003;
  _case__0005:
  _OUTA ^= 0x2;
  goto _endswitch_0003;
  _case__0006:
  _OUTA ^= 0x4;
  _OUTA ^= 0x8;
  goto _endswitch_0003;
  _case__0007:
  _OUTA ^= 0x10;
  goto _endswitch_0003;
  _endswitch_0003:
  X = X + 5;
}

