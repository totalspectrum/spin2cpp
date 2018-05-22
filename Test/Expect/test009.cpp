#include <propeller.h>
#include "test009.h"

void test009::Init(void)
{
  DIRA |= (1 << 2);
  OUTA = (OUTA & 0xffffff0f) | 0xa0;
}

