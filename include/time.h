#ifndef _TIME_H
#define _TIME_H

#pragma once

#include <compiler.h>
#include <sys/size_t.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef unsigned int clock_t;
#ifndef __FLEXC__    
extern clock_t __clkfreq_var;
#endif

/* the actual frequency the machine is running at may vary */
#define CLOCKS_PER_SEC __clkfreq_var
#define CLK_TCK __clkfreq_var

/* our time_t is the same as the Posix time_t:
 *  86400 * (number of days past the epoch) + (seconds elapsed today)
 * where "the epoch" is 00:00:00 UTC Jan. 1, 1970.
 *
 * this is often misquoted as "seconds elapsed since the epoch", but in
 * fact it is not: it ignores the existence of leap seconds
 */
typedef unsigned long time_t;

#ifndef _STRUCT_TM_DEFINED
#define _STRUCT_TM_DEFINED
/* time representing broken down calendar time */
struct tm {
  int tm_sec;
  int tm_min;
  int tm_hour;
  int tm_mday;
  int tm_mon;
  int tm_year; /* years since 1900 */
  int tm_wday; /* days since Sunday */
  int tm_yday; /* days since January 1 */
  int tm_isdst; /* if > 0, DST is in effect, if < 0 info is not known */
};
#endif

    clock_t clock(void) _IMPL("libc/sys/propeller/clock.c");
    time_t  time(time_t *) _IMPL("libc/time/time.c");
    double  difftime(time_t time2, time_t time1) _IMPL("libc/time/difftime.c");

    time_t mktime(struct tm *stm) _IMPL("libc/time/localtim.c");

    struct tm *_gmtime_r(const time_t *t, struct tm *stm) _IMPL("libc/time/localtim.c");
    struct tm *gmtime(const time_t *) _IMPL("libc/time/localtim.c");
    struct tm *_localtime_r(const time_t *, struct tm *) _IMPL("libc/time/localtim.c");
    struct tm *localtime(const time_t *) _IMPL("libc/time/localtim.c");
#define localtime_r _localtime_r
#define gmtime_r    _gmtime_r
    int _gettimezone(void) _IMPL("libc/time/localtim.c");
    
    __SIZE_TYPE__ strftime(char *s, __SIZE_TYPE__ max, const char *format, const struct tm *stm) _STRINGIO _IMPL("libc/time/strftime.c");

    char *asctime(const struct tm *stm) _STRINGIO _IMPL("libc/time/asctime.c");
    char *asctime_r(const struct tm *stm, char *buf) _STRINGIO _IMPL("libc/time/asctime.c");
    char *ctime(const time_t *timep) _STRINGIO _IMPL("libc/time/asctime.c");
    char *ctime_r(const time_t *timep, char *buf) _STRINGIO _IMPL("libc/time/asctime.c");

#if defined(_POSIX_SOURCE) || defined(_GNU_SOURCE)
    struct tm *gmtime_r(const time_t *t, struct tm *stm) _IMPL("libc/time/localtim.c");
    struct tm *localtime_r(const time_t *t, struct tm *stm) _IMPL("libc/time/localtim.c");
#endif

  /* time zone functions */
  /* set time zone to contents of environment string */
    void _tzset(void) _IMPL("libc/time/localtim.c");

#ifndef __FLEXC__    
  extern char *_tzname[2];  /* 0 is ordinary, 1 is dst */
  extern int   _timezone;   /* holds seconds west of GMT */
#endif
    
#if defined(__cplusplus)
}
#endif

#endif
