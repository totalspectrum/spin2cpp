#define __SPIN2CPP__
#include <propeller.h>
#include "test138.h"

unsigned char test138::dat[] = {
  0x44, 0x00, 0x00, 0x00, 0x55, 0x55, 
};
char *test138::Getobst(void)
{
  return (((char *)&dat[0]));
}

char *test138::Getfiller(void)
{
  return (((char *)&dat[1]));
}

uint16_t *test138::Getcmd(void)
{
  return (((uint16_t *)&dat[4]));
}

