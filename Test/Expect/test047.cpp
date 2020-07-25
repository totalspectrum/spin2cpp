#include <propeller.h>
#include "test047.h"

void test047::Test(int32_t C)
{
  int32_t 	_tmp__0000, _tmp__0001;
  _tmp__0001 = Flag;
  if (_tmp__0001 == 0) {
    goto _case__0006;
  }
  if (_tmp__0001 == 10) {
    goto _case__0007;
  }
  goto _endswitch_0005;
  _case__0006: ;
  _tmp__0000 = C;
  if (_tmp__0000 == 9) {
    goto _case__0002;
  }
  if (_tmp__0000 == 13) {
    goto _case__0003;
  }
  goto _case__0004;
  _case__0002: ;
  do {
    _OUTA = Cols++;
  } while (Cols & 0x7);
  goto _endswitch_0001;
  _case__0003: ;
  _OUTA = C;
  goto _endswitch_0001;
  _case__0004: ;
  _OUTA = Flag;
  goto _endswitch_0001;
  _endswitch_0001: ;
  goto _endswitch_0005;
  _case__0007: ;
  Cols = C % Cols;
  goto _endswitch_0005;
  _endswitch_0005: ;
  Flag = 0;
}

