#define __SPIN2CPP__
#include <propeller.h>
#include "test063.h"

void test063::Test(int32_t Exponent)
{
  int32_t 	_tmp__0000;
  _tmp__0000 = Exponent;
  if (_tmp__0000 == 0) {
    goto _case__0002;
  }
  goto _case__0003;
  _case__0002: ;
  (++Exponent);
  goto _endswitch_0001;
  _case__0003: ;
  (--Exponent);
  goto _endswitch_0001;
  _endswitch_0001: ;
}

