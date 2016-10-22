#include <propeller.h>
#include "test140.h"

uint8_t test140::dat[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
};
int32_t test140::Getval(void)
{
  int32_t result = 0;
  result = ((uint16_t *)((int32_t *)&dat[4])[0])[0];
  return result;
}

