#include <propeller.h>
#include "test48.h"

void test48::Setcolors(int32_t Colorptr)
{
  int32_t	I, Fore, Back;
  for(I = 0; I <= 7; I++) {
    Fore = ((char *)Colorptr)[(I << 1)] << 2;
    OUTA = (OUTA & 0xffffff00) | ((Fore & 0xff) << 0);
  }
}

void test48::Stop(void)
{
  OUTA &= (~(255 << 0));
}

