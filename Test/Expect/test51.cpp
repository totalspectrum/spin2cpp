#include <propeller.h>
#include "test51.h"

int32_t test51::Run(void)
{
  int32_t result = 0;
  _OUTA = Len((int32_t)"hello, world\r\n");
  return result;
}

int32_t test51::Len(int32_t Stringptr)
{
  int32_t result = 0;
  return strlen((char *) Stringptr);
}

