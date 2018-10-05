#include <propeller.h>
#include "btest007.h"

void btest007::program(void)
{
  const char *	x;
  int32_t 	y, z;
  x = "hello";
  y = 2;
  z = y * y;
  basic_print_string(0, x);
  basic_print_char(0, 9);
  basic_print_integer(0, y);
  basic_print_char(0, 9);
  basic_print_integer(0, z);
  basic_print_nl(0);
}

