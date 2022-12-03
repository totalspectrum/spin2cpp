#define __SPIN2CPP__
#include <propeller.h>
#include "test166.h"

int32_t test166::Check(int32_t Source, char *Addr)
{
  int32_t Val;
  Val = (((Source == 'F') || (Source == 'R')) ? Foo((int32_t)Addr) : Addr[Source]);
  return Val;
}

int32_t test166::Foo(int32_t Addr)
{
  return (_OUTA + Addr);
}

