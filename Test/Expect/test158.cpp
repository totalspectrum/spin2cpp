#define __SPIN2CPP__
#include <propeller.h>
#include "test158.h"

int32_t test158::Check(int32_t Exponent)
{
  int32_t 	_tmp__0000;
  _tmp__0000 = Exponent;
  if ((_tmp__0000 >= 0) && (_tmp__0000 <= 3)) {
    goto _case__0002;
  }
  if ((_tmp__0000 >= 4) && (_tmp__0000 <= 5)) {
    goto _case__0003;
  }
  goto _case__0004;
  _case__0002: ;
  return 1;
  goto _endswitch_0001;
  _case__0003: ;
  return 2;
  goto _endswitch_0001;
  _case__0004: ;
  return 3;
  goto _endswitch_0001;
  _endswitch_0001: ;
}

