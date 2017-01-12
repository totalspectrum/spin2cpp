#include <propeller.h>
#include "test73.h"

char test73::dat[] = {
  0x00, 0x60, 0x00, 0x00, 
};
int32_t test73::Flip(int32_t X)
{
  OUTA = (OUTA & 0xfffffff0) | 0xc;
  return ((int32_t *)&dat[0])[0];
}

