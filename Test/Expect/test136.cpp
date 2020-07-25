#include <propeller.h>
#include "test136.h"

void test136::Doone(int32_t X)
{
  int32_t 	_tmp__0000;
  _tmp__0000 = X;
  if (_tmp__0000 == 0) {
    goto _case__0002;
  }
  if (_tmp__0000 == 1) {
    goto _case__0003;
  }
  goto _case__0004;
  _case__0002: ;
  Dotwo(1);
  goto _endswitch_0001;
  _case__0003: ;
  Dotwo(2);
  goto _endswitch_0001;
  _case__0004: ;
  Dotwo(3);
  goto _endswitch_0001;
  _endswitch_0001: ;
}

void test136::Dotwo(int32_t X)
{
  _OUTA = X;
}

