#include <propeller.h>
#include "test51.h"

void test51::Run(void)
{
  OUTA = Len((char *)"hello, world\r\n");
}

int32_t test51::Len(char *Stringptr)
{
  return strlen(Stringptr);
}

