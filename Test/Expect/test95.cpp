#include <propeller.h>
#include "test95.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif


//
// and here are the C versions of the methods
//
int32_t test95::Square(int32_t x)
{
  return x*x;
}


