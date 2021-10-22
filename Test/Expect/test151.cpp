#define __SPIN2CPP__
#include <propeller.h>
#include "test151.h"

// this function expects a string
int32_t test151::Countspaces(const char *A)
{
  int32_t 	N;
  int32_t 	C;
  N = 0;
  while (1) {
    C = (A++)[0];
    if (C == 0) {
      break;
    }
    if (C == ' ') {
      (N++);
    }
  }
  return N;
}

// this function just expects an integer
void test151::Addup(int32_t X)
{
  A = A + X;
}

void test151::Demo(void)
{
  A = Countspaces("hello, world");
  B = Countspaces("");
  Addup('x');
}

