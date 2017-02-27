#include <propeller.h>
#include "test51.h"

void test51::Run(void)
{
  OUTA = Len("hello, world\r\n");
}

int32_t test51::Len(const char *Stringptr)
{
  return strlen(Stringptr);
}

