#include <propeller.h>
#include "test152.h"

int32_t test152::Hexdigit(int32_t X)
{
  switch(X) {
  case 'a':
  case 'b':
  case 'c':
  case 'd':
  case 'e':
  case 'f':
    return 1;
    break;
  case 'A':
  case 'B':
  case 'C':
  case 'D':
  case 'E':
  case 'F':
    return 1;
    break;
  }
  return 0;
}

