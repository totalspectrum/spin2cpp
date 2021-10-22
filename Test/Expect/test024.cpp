#define __SPIN2CPP__
#include <propeller.h>
#include "test024.h"

void test024::Unlock(void)
{
  int32_t 	_tmp__0000;
  _OUTA = ( ( (_tmp__0000 = X), (X = 0) ), _tmp__0000 );
}

