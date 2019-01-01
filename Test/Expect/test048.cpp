#include <propeller.h>
#include "test048.h"

void test048::Setcolors(char *Colorptr)
{
  int32_t 	I, Fore;
  int32_t 	Back;
  for(I = 0; I < 8; I++) {
    Fore = Colorptr[(I << 1)] << 2;
    _OUTA = (_OUTA & 0xffffff00) | ((Fore & 0xff) << 0);
  }
}

void test048::Stop(void)
{
  _OUTA &= (~(255 << 0));
}

