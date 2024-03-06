#include <stdlib.h>
#define __SPIN2CPP__
#include <propeller.h>
#include "test169.h"

void test169::Func1(int32_t A, int32_t B)
{
  int32_t 	_tmp__0000;
  _tmp__0000 = abs((A % 3));
  if (_tmp__0000 == 0) {
    goto _case__0002;
  }
  if (_tmp__0000 == 1) {
    goto _case__0003;
  }
  if (_tmp__0000 == 2) {
    goto _case__0004;
  }
  if (_tmp__0000 == (abs(B))) {
    goto _case__0005;
  }
  goto _endswitch_0001;
  _case__0002: ;
  _OUTA = 1;
  goto _endswitch_0001;
  _case__0003: ;
  _OUTA = 2;
  goto _endswitch_0001;
  _case__0004: ;
  _OUTA = 3;
  goto _endswitch_0001;
  _case__0005: ;
  _OUTA = 4;
  goto _endswitch_0001;
  _endswitch_0001: ;
}

