#include <propeller.h>
#include "test138.h"

uint8_t test138::dat[] = {
  0x44, 0x00, 0x00, 0x00, 0x55, 0x55, 
};
int32_t test138::Getobst(void)
{
  return (int32_t)(((uint8_t *)&dat[0]));
}

int32_t test138::Getfiller(void)
{
  return (int32_t)(((uint8_t *)&dat[1]));
}

int32_t test138::Getcmd(void)
{
  return (int32_t)(((uint16_t *)&dat[4]));
}

