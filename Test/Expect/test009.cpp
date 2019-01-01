#include <propeller.h>
#include "test009.h"

void test009::Init(void)
{
  _DIRA |= (1 << 2);
  _OUTA = (_OUTA & 0xffffff0f) | 0xa0;
}

