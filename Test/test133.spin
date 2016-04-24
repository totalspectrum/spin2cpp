''
'' test for {++ } at top level
''
VAR
  byte txpin
  byte rxpin
  long baud
  long txmask
  long bitcycles

#ifdef __SPIN2CPP__
'' we will be using stdio, so force it to be
'' included
{++
#include <stdio.h>
}
#endif

''
'' this is the putx function
''

PUB putx(c)
  {++
  putchar(c);
  }

PUB start(a)
  return 0
