#include <time.h>
#include <sys/thread.h>
#include <propeller.h>
#include "cog.h"

void
sleep(unsigned int n)
{
  unsigned waitcycles;
  unsigned second = _clkfreq;

  waitcycles = getcnt();
  while (n > 0) 
    {
      waitcycles += second;
      __napuntil(waitcycles);
      --n;
    }
}
