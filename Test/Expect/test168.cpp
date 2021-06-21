#include <propeller.h>
#include "test168.h"

void test168::Func1(void)
{
  C = ( (B = 291), 291 );
}

void test168::Func2(void)
{
  int32_t 	_temp__0000;
  C = ( ( (_temp__0000 = B + 32), (B = _temp__0000) ), _temp__0000 );
}

