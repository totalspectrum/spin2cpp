#include <sys/time.h>
#include <sys/rtc.h>

int
gettimeofday(struct timeval *tv, void *unused)
{
  return (*_rtc_gettime)(tv);
}
