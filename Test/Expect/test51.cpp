#include <propeller.h>
#include "test51.h"

int32_t test51::run(void)
{
  int32_t result = 0;
  _OUTA = len((int32_t)"hello, world\r\n");
  return result;
}

int32_t test51::len(int32_t stringptr)
{
  int32_t result = 0;
  return strlen((char *) stringptr);
}

