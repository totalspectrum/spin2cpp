#include <time.h>
#include <sys/time.h>

time_t
time(time_t *tp)
{
    struct timeval tv;
    time_t now;
    gettimeofday(&tv, (void *)0);
    now = tv.tv_sec;
    if (tp)
        *tp = now;
    return now;
}
