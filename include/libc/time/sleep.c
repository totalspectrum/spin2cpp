#include <time.h>
#include <propeller.h>
#include <unistd.h>
#include "cog.h"

unsigned int
sleep(unsigned int n)
{
  unsigned waitcycles;
  unsigned second = _clkfreq;

  waitcycles = getcnt();
  while (n > 0) 
    {
      waitcycles += second;
      waitcnt(waitcycles);
      --n;
    }
  return 0;
}
