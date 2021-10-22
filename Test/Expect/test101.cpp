#define __SPIN2CPP__
#include <propeller.h>
#include "test101.h"

void test101::Foo(void)
{
  _OUTA = (int32_t)0xbf800000;
}

