#include <time.h>
#include <sys/thread.h>
#include <unistd.h>
#include <propeller.h>
#include "cog.h"

int
usleep(unsigned int n)
{
  unsigned waitcycles;
  unsigned usecond = _clkfreq/1000000;

  waitcycles = getcnt() + n*usecond;
  waitcnt(waitcycles);

  return 0;
}
