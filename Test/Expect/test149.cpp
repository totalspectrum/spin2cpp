#include <propeller.h>
#include "test149.h"

int32_t test149::Sgncomp(int32_t X, int32_t Y)
{
  return ((X > Y) ? 1 : -(X < Y));
}

// using the \ operator to swap a and b
void test149::Swap(void)
{
  int32_t 	_tmp__0000;
  A = ( ( (_tmp__0000 = B), (B = A) ), _tmp__0000 );
}

