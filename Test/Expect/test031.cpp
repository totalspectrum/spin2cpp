#include <propeller.h>
#include "test031.h"

void test031::Fun(int32_t Y)
{
  int32_t 	_tmp__0001;
  _tmp__0001 = X + Y;
  if (_tmp__0001 == 10) {
    goto _case__0003;
  }
  if (_tmp__0001 == (A * 2)) {
    goto _case__0004;
  }
  goto _case__0005;
  _case__0003: ;
  _OUTA ^= 0x1;
  goto _endswitch_0002;
  _case__0004: ;
  _OUTA ^= 0x2;
  _OUTA ^= 0x4;
  goto _endswitch_0002;
  _case__0005: ;
  _OUTA ^= 0x8;
  goto _endswitch_0002;
  _endswitch_0002: ;
  X = X + 5;
}

