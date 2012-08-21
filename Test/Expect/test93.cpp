#include <propeller.h>
#include "test93.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif

uint8_t test93::dat[] = {
  0x00, 0x00, 0x00, 0x00, 
};

#include <stdio.h>

int foo() { return 0; }


int bar( int x )
{
  return x - 1;
}

