#include <time.h>

/*
 * calculate seconds between two timestamps
 * BUG: this function ignores the existence of
 * leapseconds. It's in good company (Posix basically
 * pretends leapseconds don't exist) but that does mean
 * its results are bogus if there is a leapsecond in the
 * interval
 */
double
difftime(time_t t2, time_t t1)
{
  if (t1 <= t2)
    return (double)(t2-t1);
  else
    return -(double)(t1-t2);
}
