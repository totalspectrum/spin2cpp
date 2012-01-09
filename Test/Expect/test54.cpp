#include <propeller.h>
#include "test54.h"

uint8_t test54::dat[] = {
  0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
};
int32_t test54::getfoo(void)
{
  int32_t result = 0;
  return (int32_t)(&(*(int32_t *)&dat[4]));
}

