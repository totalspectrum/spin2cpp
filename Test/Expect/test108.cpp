#include <stdlib.h>
#include <propeller.h>
#include "test108.h"

void test108::Main(void)
{
  DIRA |= (255 << 16);
  Show(12);
  OUTA ^= 0xff0000;
}

void test108::Show(int32_t X)
{
  OUTA = (OUTA & 0xff00ffff) | ((X & 0xff) << 16);
}

