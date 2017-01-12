#include <propeller.h>
#include "test122.h"

void test122::Test(void)
{
  OUTA = (Voidfunc(1), 0);
}

void test122::Voidfunc(int32_t Y)
{
  DIRA = Y;
}

