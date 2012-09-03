#include <propeller.h>
#include "test93.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
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

