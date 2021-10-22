#define __SPIN2CPP__
#include <propeller.h>
#include "btest007.h"

void btest007::program(void)
{
  const char *	x;
  int32_t 	y, z;
  x = "hello";
  y = 2;
  z = y * y;
  printf("%s\t%d\t%d\n", x, y, z);
}

