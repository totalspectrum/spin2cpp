#include <sys/time.h>
#include <propeller.h>

time_t
time(time_t *tp)
{
    struct timeval tv;
    time_t now;
    gettimeofday(&tv, NULL);
    now = tv.tv_sec;
    if (tp)
        *tp = now;
    return now;
}
