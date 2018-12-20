#include <sys/time.h>
#include <sys/rtc.h>

int
settimeofday(const struct timeval *tv, const void *unused)
{
  return (*_rtc_settime)(tv);
}
