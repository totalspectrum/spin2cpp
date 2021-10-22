#define __SPIN2CPP__
#include <propeller.h>
#include "test156.h"

#define Yield__() __asm__ volatile( "" ::: "memory" )
void test156::Main(void)
{
  while (1) {
    Yield__();
  }
}

/* 
comment; this caused confusion in earlier versions of fastspin
 */
