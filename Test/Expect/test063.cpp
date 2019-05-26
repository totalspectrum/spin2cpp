#include <propeller.h>
#include "test063.h"

void test063::Test(int32_t Exponent)
{
  int32_t 	_tmp__0001;
  _tmp__0001 = Exponent;
  if (_tmp__0001 == 0) {
    goto _case__0003;
  }
  goto _case__0004;
  _case__0003:
  (++Exponent);
  goto _endswitch_0002;
  _case__0004:
  (--Exponent);
  goto _endswitch_0002;
  _endswitch_0002:
}

