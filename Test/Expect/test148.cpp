#include <propeller.h>
#include "test148.h"

void test148::Func(int32_t Y)
{
  OUTA = Y;
}

void test148::Setclr(void)
{
  int32_t 	_tmp__0001;
  Func(( ( (_tmp__0001 = A), (A = 0) ), _tmp__0001 ));
}

