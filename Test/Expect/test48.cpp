#include <propeller.h>
#include "test48.h"

int32_t test48::Setcolors(int32_t Colorptr)
{
  int32_t result = 0;
  int32_t	I, Fore, Back;
  I = 0;
  do {
    Fore = (((uint8_t *)Colorptr)[(I << 1)] << 2);
    _OUTA = (_OUTA & 0xffffff00) | ((Fore << 0) & 0x000000ff);
    I = (I + 1);
  } while (I <= 7);
  return result;
}

int32_t test48::Stop(void)
{
  int32_t result = 0;
  _OUTA &= ~(255<<0);
  return result;
}

