#define __SPIN2CPP__
#include <propeller.h>
#include "test167.h"

int32_t test167::Testup(int32_t I)
{
  int32_t 	_look__0001;
  int32_t R;
  R = ( (_look__0001 = I++), ((_look__0001 == 804) ? 99 : (((3 <= _look__0001) && (_look__0001 <= 803)) ? 100 + (_look__0001 - 3) : ((_look__0001 == 2) ? 2 : ((_look__0001 == 1) ? 1 : 0)))) );
  return R;
}

