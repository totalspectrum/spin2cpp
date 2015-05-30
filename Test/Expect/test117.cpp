/*  comment test  */
#include <propeller.h>
#include "test117.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

int32_t test117::Getit(void)
{
  int32_t	I;
  // start something
  A = 0;
  // count up
  for(I = 1; I <= 5; I++) {
    // update a
    A = A + I;
  }
  // now all done
  return 0;
}

/* 
  This comment is at end of file
 */
