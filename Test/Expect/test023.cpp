#include <propeller.h>
#include "test023.h"

int32_t test023::Start(void)
{
  int32_t	X;
  int32_t R = 0;
  //
  // just a comment
  //
  X = locknew();
  R = lockclr(X);
  return R;
}

