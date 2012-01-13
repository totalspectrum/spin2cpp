#include <propeller.h>
#include "test56.h"

uint8_t test56::dat[] = {
  0x00, 0x00, 0x80, 0x3f, 
};
int32_t test56::getval(void)
{
  int32_t result = 0;
  return (*(int32_t *)&dat[0]);
}

