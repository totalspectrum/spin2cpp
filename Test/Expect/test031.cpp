#define __SPIN2CPP__
#include <propeller.h>
#include "test031.h"

void test031::Fun(int32_t Y)
{
  int32_t 	_tmp__0000;
  _tmp__0000 = X + Y;
  if (_tmp__0000 == 10) {
    goto _case__0002;
  }
  if (_tmp__0000 == (A * 2)) {
    goto _case__0003;
  }
  goto _case__0004;
  _case__0002: ;
  _OUTA ^= 0x1;
  goto _endswitch_0001;
  _case__0003: ;
  _OUTA ^= 0x2;
  _OUTA ^= 0x4;
  goto _endswitch_0001;
  _case__0004: ;
  _OUTA ^= 0x8;
  goto _endswitch_0001;
  _endswitch_0001: ;
  X = X + 5;
}

