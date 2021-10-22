//
// another test
//
/* 
  with an inline comment
 */
#define __SPIN2CPP__
#include <propeller.h>
#include "test007.h"

int32_t test007::Donext(void)
{
  Count = Count + 1;
  return Count;
}

