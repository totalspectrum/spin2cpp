#include <propeller.h>
#include "test051.h"

void test051::Run(void)
{
  OUTA = Len("hello, world\r\n");
}

int32_t test051::Len(const char *Stringptr)
{
  return strlen(Stringptr);
}

