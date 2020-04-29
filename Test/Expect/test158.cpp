#include <propeller.h>
#include "test158.h"

int32_t test158::Check(int32_t Exponent)
{
  int32_t 	_tmp__0001;
  _tmp__0001 = Exponent;
  if ((_tmp__0001 >= 0) && (_tmp__0001 <= 3)) {
    goto _case__0003;
  }
  if ((_tmp__0001 >= 4) && (_tmp__0001 <= 5)) {
    goto _case__0004;
  }
  goto _case__0005;
  _case__0003: ;
  return 1;
  goto _endswitch_0002;
  _case__0004: ;
  return 2;
  goto _endswitch_0002;
  _case__0005: ;
  return 3;
  goto _endswitch_0002;
  _endswitch_0002: ;
}

