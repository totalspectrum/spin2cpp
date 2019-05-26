#include <propeller.h>
#include "test136.h"

void test136::Doone(int32_t X)
{
  int32_t 	_tmp__0001;
  _tmp__0001 = X;
  if (_tmp__0001 == 0) {
    goto _case__0003;
  }
  if (_tmp__0001 == 1) {
    goto _case__0004;
  }
  goto _case__0005;
  _case__0003:
  Dotwo(1);
  goto _endswitch_0002;
  _case__0004:
  Dotwo(2);
  goto _endswitch_0002;
  _case__0005:
  Dotwo(3);
  goto _endswitch_0002;
  _endswitch_0002:
}

void test136::Dotwo(int32_t X)
{
  _OUTA = X;
}

