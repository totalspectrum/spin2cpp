#include <propeller.h>
#include "test51.h"

void test51::Run(void)
{
  OUTA = Len((int32_t)"hello, world\r\n");
}

int32_t test51::Len(int32_t Stringptr)
{
  return strlen((char *) Stringptr);
}

