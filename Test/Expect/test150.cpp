#define __SPIN2CPP__
#include <propeller.h>
#include "test150.h"

const char *test150::Ptr(const char *A)
{
  return A;
}

const char *test150::Greet1(void)
{
  return Ptr("hello");
}

const char *test150::Greet2(void)
{
  return Ptr("hi");
}

char *test150::Fluff(void)
{
  return (Buf);
}

