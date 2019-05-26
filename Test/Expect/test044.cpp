#include <propeller.h>
#include "test044.h"

void test044::Fun(int32_t X, int32_t Y)
{
  int32_t 	_tmp__0001, _tmp__0005;
  _tmp__0005 = X;
  if (_tmp__0005 == 10) {
    goto _case__0007;
  }
  if (_tmp__0005 == 20) {
    goto _case__0008;
  }
  goto _case__0009;
  _case__0007:
  _tmp__0001 = Y;
  if (_tmp__0001 == 0) {
    goto _case__0003;
  }
  if (_tmp__0001 == 1) {
    goto _case__0004;
  }
  goto _endswitch_0002;
  _case__0003:
  _OUTA ^= 0x1;
  goto _endswitch_0002;
  _case__0004:
  _OUTA ^= 0x2;
  goto _endswitch_0002;
  _endswitch_0002:
  goto _endswitch_0006;
  _case__0008:
  _OUTA ^= 0x4;
  goto _endswitch_0006;
  _case__0009:
  _OUTA ^= 0x8;
  goto _endswitch_0006;
  _endswitch_0006:
}

