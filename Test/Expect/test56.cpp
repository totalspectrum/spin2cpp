#include <propeller.h>
#include "test56.h"

uint8_t test56::dat[] = {
  0x00, 0x00, 0x80, 0x3f, 
};
int32_t test56::Getval(void)
{
  return ((int32_t *)&dat[0])[0];
}

