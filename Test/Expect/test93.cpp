#include <propeller.h>
#include "test93.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif
INLINE__ int32_t PostFunc__(int32_t *x, int32_t y) { int32_t t = *x; *x = y; return t; }
#define PostEffect__(X, Y) PostFunc__(&(X), (Y))

uint8_t test93::dat[] = {
  0x00, 0x00, 0x00, 0x00, 
};

#include <stdio.h>

int foo() { return 0; }


int bar( int x )
{
  return x - 1;
}

