/* 
  apparently openspin accepts code that forgets the initial con
 */
#include <propeller.h>
#include "test130.h"

#define Yield__() __asm__ volatile( "" ::: "memory" )
void test130::Main(void)
{
  while (1) {
    Yield__();
  }
}

