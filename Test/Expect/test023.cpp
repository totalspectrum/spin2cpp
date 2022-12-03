#define __SPIN2CPP__
#include <propeller.h>
#include "test023.h"

int32_t test023::Start(void)
{
  int32_t 	X;
  int32_t R;
  //
  // just a comment
  //
  X = locknew();
  R = lockclr(X);
  return R;
}

