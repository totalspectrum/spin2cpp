#include <propeller.h>
#include "test83.h"

uint8_t test83::dat[] = {
  0x01, 0x00, 0x00, 0x00, 0xf0, 0x03, 0xbc, 0xa0, 0x44, 0x33, 0x22, 0x11, 
};
int32_t test83::Start(void)
{
  int32_t result = 0;
  cognew((int32_t)(&(*(int32_t *)&dat[4])), 0);
  return result;
}

