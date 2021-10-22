/*  comment test  */
#define __SPIN2CPP__
#include <propeller.h>
#include "test117.h"

int32_t test117::Getit(void)
{
  int32_t 	I;
  // start something
  A = 0;
  // count up
  for(I = 1; I < 6; I++) {
    // update a
    A = A + I;
  }
  // now all done
  return 0;
}

/* 
  This comment is at end of file
 */
