#include <propeller.h>
#include "test106.h"

uint8_t volatile test106::dat[] = {
  0x00, 0x00, 0x00, 0x00, 
};
int32_t test106::Foo(void)
{
  return (((int32_t *)&dat[0])[0] + 1);
}

