#include <sys/time.h>
#include <propeller.h>

time_t
time(time_t *tp)
{
  time_t now;

  now = getcnt() / CLKFREQ;
  if (tp)
    *tp = now;
  return now;
}
