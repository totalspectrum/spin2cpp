#include <sys/time.h>
#include <sys/rtc.h>

time_t
time(time_t *tp)
{
  struct timeval tv;
  time_t now;

  (*_rtc_gettime)(&tv);
  now = tv.tv_sec;
  if (tp)
    *tp = now;
  return now;
}
