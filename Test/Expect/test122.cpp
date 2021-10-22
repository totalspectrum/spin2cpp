#define __SPIN2CPP__
#include <propeller.h>
#include "test122.h"

void test122::Test(void)
{
  _OUTA = (Voidfunc(1), 0);
}

void test122::Voidfunc(int32_t Y)
{
  _DIRA = Y;
}

