#include <propeller.h>
#include "test09.h"

void test09::Init(void)
{
  DIRA |= (1 << 2);
  OUTA = (OUTA & 0xffffff0f) | 0xa0;
}

