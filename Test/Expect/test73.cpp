#include <propeller.h>
#include "test73.h"

uint8_t test73::dat[] = {
  0x00, 0x60, 0x00, 0x00, 
};
int32_t test73::Flip(int32_t X)
{
  int32_t result = 0;
  _OUTA = (_OUTA & 0xfffffff0) | ((12 << 0) & 0x0000000f);
  return (*(int32_t *)&dat[0]);
}

