#define __SPIN2CPP__
#include <propeller.h>
#include "test073.h"

unsigned char test073::dat[] = {
  0x00, 0x60, 0x00, 0x00, 
};
int32_t test073::Flip(int32_t X)
{
  _OUTA = (_OUTA & 0xfffffff0) | 0xc;
  return ((int32_t *)&dat[0])[0];
}

