#include <propeller.h>
#include "test047.h"

void test047::Test(int32_t C)
{
  int32_t 	_tmp__0001, _tmp__0006;
  _tmp__0006 = Flag;
  if (_tmp__0006 == 0) {
    goto _case__0008;
  }
  if (_tmp__0006 == 10) {
    goto _case__0009;
  }
  goto _endswitch_0007;
  _case__0008: ;
  _tmp__0001 = C;
  if (_tmp__0001 == 9) {
    goto _case__0003;
  }
  if (_tmp__0001 == 13) {
    goto _case__0004;
  }
  goto _case__0005;
  _case__0003: ;
  do {
    _OUTA = Cols++;
  } while (Cols & 0x7);
  goto _endswitch_0002;
  _case__0004: ;
  _OUTA = C;
  goto _endswitch_0002;
  _case__0005: ;
  _OUTA = Flag;
  goto _endswitch_0002;
  _endswitch_0002: ;
  goto _endswitch_0007;
  _case__0009: ;
  Cols = C % Cols;
  goto _endswitch_0007;
  _endswitch_0007: ;
  Flag = 0;
}

