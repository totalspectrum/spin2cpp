#define __SPIN2CPP__
#include <propeller.h>
#include "test074.h"

unsigned char test074::dat[] = {
  0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 
};
int32_t test074::Func(void)
{
  return ((int32_t *)&dat[8])[0];
}

