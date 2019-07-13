#include <string.h>
#include <propeller.h>
#include "test051.h"

void test051::Run(void)
{
  _OUTA = Len("hello, world\r\n");
}

int32_t test051::Len(const char *Stringptr)
{
  return strlen(Stringptr);
}

