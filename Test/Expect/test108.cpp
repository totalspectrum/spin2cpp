#define __SPIN2CPP__
#include <propeller.h>
#include "test108.h"

void test108::Main(void)
{
  _DIRA |= (255 << 16);
  Show(12);
  _OUTA ^= 0xff0000;
}

void test108::Show(int32_t X)
{
  _OUTA = (_OUTA & 0xff00ffff) | ((X & 0xff) << 16);
}

