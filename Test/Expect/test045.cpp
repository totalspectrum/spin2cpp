#define __SPIN2CPP__
#include <propeller.h>
#include "test045.h"

void test045::Fun(int32_t X, int32_t Y)
{
  int32_t 	_tmp__0000, _tmp__0001;
  _tmp__0001 = X;
  if (_tmp__0001 == 0) {
    goto _case__0005;
  }
  if (_tmp__0001 == 20) {
    goto _case__0006;
  }
  goto _case__0007;
  _case__0005: ;
  _tmp__0000 = Y;
  if (_tmp__0000 == 0) {
    goto _case__0002;
  }
  if (_tmp__0000 == 1) {
    goto _case__0003;
  }
  goto _endswitch_0001;
  _case__0002: ;
  _OUTA ^= 0x1;
  goto _endswitch_0001;
  _case__0003: ;
  _OUTA ^= 0x2;
  goto _endswitch_0001;
  _endswitch_0001: ;
  goto _endswitch_0004;
  _case__0006: ;
  _OUTA ^= 0x4;
  goto _endswitch_0004;
  _case__0007: ;
  _OUTA ^= 0x8;
  goto _endswitch_0004;
  _endswitch_0004: ;
}

