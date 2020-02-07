/* taken from Dale Schumacher's dLibs library */
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <sys/thread.h>

static char ctime_buf[28];

char *_asctime_r(const struct tm *time, char *buf)
{
  strftime(buf, 26, "%a %b %d %H:%M:%S %Y\n", time);
  return buf;
}

char *asctime(const struct tm *time)
/*
 *      Convert <time> structure value to a string.  The same format, and
 *      the same internal buffer, as for ctime() is used for this function.
 */
{
  return _asctime_r(time, &ctime_buf[0]);
}

char *ctime(const time_t *rawtime)
/*
 *      Convert <rawtime> to a string.  A 26 character fixed field string
 *      is created from the raw time value.  The following is an example
 *      of what this string might look like:
 *              "Wed Jul  8 18:43:07 1987\n\0"
 *      A 24-hour clock is used. A pointer to the formatted
 *      string, which is held in an internal buffer, is
 *      returned.
 */
{
        return(asctime(localtime(rawtime)));
}
