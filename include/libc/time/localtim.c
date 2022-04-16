/* from the MiNT library */
/* mktime, localtime, gmtime */
/* written by Eric R. Smith and placed in the public domain */

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>

#define toint(c) ((c)-'0')

#if 0
#include <stdio.h>
static void
DEBUG_TM(const char *nm, struct tm *tm)
{
	char buf[100];

	(void)strftime(buf, 100, "%c %z", tm);
	printf("%s: %s\n", nm, buf);
}
#else
#define DEBUG_TM(nm, tm)
#endif

#define SECS_PER_MIN    (60L)
#define SECS_PER_HOUR   (3600L)
#define SECS_PER_DAY    (86400L)
#define SECS_PER_YEAR   (31536000L)
#define SECS_PER_LEAP_YEAR   (31536000L + SECS_PER_DAY)
#define SECS_PER_FOUR_YEARS (4*SECS_PER_YEAR + SECS_PER_DAY)

#define START_OF_2012 (1325376000UL)
#define START_OF_2100 (4102444800UL)

int _timezone = -1;	/* holds # seconds west of GMT */

static int
days_per_mth[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

static int
mth_start[13] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 };

time_t __tzoffset(char *s, int *hasdst);
int __indst(const struct tm *t);

static int dst = -1;	/* whether dst holds in current timezone */

/*
 * FIXME: none of these routines is very efficient. Also, none of them
 * handle dates before Jan 1, 1970.
 *
 */

int
_is_leap_year(int y)
{
  if ( (0 == y % 4) ) {
    if (0 == y % 100)
      return (0 == y % 400);
    return 1;
  }
  return 0;
}

/*
 * mktime: take a time structure representing the local time (such as is
 *  returned by localtime() and convert it into the standard representation
 *  (as seconds since midnight Jan. 1 1970, GMT).
 *
 * Note that time() sends us such a structure with tm_yday and tm_wday
 * undefined, so we shouldn't count on these being correct!
 */

time_t
mktime(struct tm *t)
{
        time_t s;
        int yday;   /* day of the year */
	int year;   /* full year */
	int leaps;

DEBUG_TM("mktime", t);
        if (t->tm_year < 70)      /* year before 1970 */
                return (time_t) -1;

/* calculate tm_yday here */
	year = 1900 + t->tm_year;
	yday = (t->tm_mday - 1) + mth_start[t->tm_mon] + /* leap year correction */
	  ( (_is_leap_year(year) ) ? (t->tm_mon > 1) : 0 );

	s = (t->tm_sec)+(t->tm_min*SECS_PER_MIN)+(t->tm_hour*SECS_PER_HOUR)
	  + (yday*SECS_PER_DAY)+((year - 1970)*SECS_PER_YEAR);

	/* add in the number of leap years since 1970 */
	/* note that 2000 is a leap year, but 2100, 2200, and 2300 are not */
	leaps = (year - 1969)/4;
	if (year > 2000) {
	  leaps -= (year - 2000)/100;
	}
	s += leaps*SECS_PER_DAY;

/* Now adjust for the time zone and possible daylight savings time */
/* note that we have to call tzset() every time; see 1003.1 sect 8.1.1 */
	_tzset();

        s += _timezone;
        if (dst == 1 && __indst(t))
                s -= SECS_PER_HOUR;

        return s;
}


struct tm *_gmtime_r(const time_t *t, struct tm *stm)
{
        time_t  time = *t;
        int     year, mday, i;
	time_t  yearlen;

        stm->tm_wday = (int) (((time/SECS_PER_DAY) + 4) % 7);

	if (time >= START_OF_2012) {
	  time -= START_OF_2012;
	  year = 2012;
	} else {
	  year = 1970;
	}

	for(;;) {
	  if (_is_leap_year(year))
	    yearlen = SECS_PER_LEAP_YEAR;
	  else
	    yearlen = SECS_PER_YEAR;
	  if (time < yearlen)
	    break;
	  year++;
	  time -= yearlen;
	}
	/* at this point we should have the seconds left in the year */
        stm->tm_year = year - 1900;
        mday = stm->tm_yday = (int)(time/SECS_PER_DAY);

        days_per_mth[1] = _is_leap_year(year) ? 29 : 28;
        for (i = 0; mday >= days_per_mth[i]; i++)
                mday -= days_per_mth[i];
        stm->tm_mon = i;
        stm->tm_mday = mday + 1;

        time = time % SECS_PER_DAY;
        stm->tm_hour = (int) (time/SECS_PER_HOUR);
        time = time % SECS_PER_HOUR;
        stm->tm_min = (int) (time/SECS_PER_MIN);
        stm->tm_sec = (int) (time % SECS_PER_MIN);
        stm->tm_isdst = 0;

DEBUG_TM("gmtime", stm);
        return stm;
}

/* given a standard time, convert it to a local time */

struct tm *_localtime_r(const time_t *t, struct tm *stm)
{
        time_t gmsecs;  /*time in GMT */ 

	_tzset();
	gmsecs = *t;
	if ((int)gmsecs > _timezone)
	  gmsecs = gmsecs - _timezone;
        stm = _gmtime_r(&gmsecs, stm);

        stm->tm_isdst = (dst == -1) ? -1 : 0;

        if (dst == 1 && __indst((const struct tm *)stm)) {
	   /* daylight savings time in effect */
                stm->tm_isdst = 1;
                if (++stm->tm_hour > 23) {
                        stm->tm_hour -= 24;
                        stm->tm_wday = (stm->tm_wday + 1) % 7;
                        stm->tm_yday++;
                        stm->tm_mday++;
                        if (stm->tm_mday > days_per_mth[stm->tm_mon]) {
                                stm->tm_mday = 1;
                                stm->tm_mon++;
                        }
                }
        }
	DEBUG_TM("localtime", stm);
        return stm;
}

struct tm *localtime(const time_t *t)
{
	static struct tm time_temp;
	return _localtime_r(t, &time_temp);
}

struct tm *gmtime(const time_t *t)
{
	static struct tm time_temp;
	return _gmtime_r(t, &time_temp);
}

/*
 * there appears to be a conflict between Posix and ANSI; the former
 * mandates a "tzset()" function that gets called whenever time()
 * does, and which sets some global variables. ANSI wants none of
 * this. Several Unix implementations have tzset(), and few people are
 * going to be hurt by it, so it's included here but named as _tzset
 * so it does not violate ANSI
 */

/* set the timezone and dst flag to the local rules. Also sets the
   global variable tzname to the names of the timezones
 */

char *_tzname[2] = {"UCT", "UCT"};

void
_tzset(void)
{
	_timezone = __tzoffset(getenv("TZ"), &dst);
}

/*
 * determine the difference, in seconds, between the given time zone
 * and Greenwich Mean. As a side effect, the integer pointed to
 * by hasdst is set to 1 if the given time zone follows daylight
 * savings time, 0 if there is no DST.
 *
 * Time zones are given as strings of the form
 * "[TZNAME][h][:m][TZDSTNAME]" where h:m gives the hours:minutes
 * east of GMT for the timezone (if [:m] does not appear, 0 is assumed).
 * If the final field, TZDSTNAME, appears, then the time zone follows
 * daylight savings time.
 *
 * Example: EST5EDT would represent the N. American Eastern time zone
 *          CST6CDT would represent the N. American Central time zone
 *          NFLD3:30NFLD would represent Newfoundland time (one and a
 *              half hours ahead of Eastern).
 *          OZCST-9:30 would represent the Australian central time zone.
 *              (which, so I hear, doesn't have DST).
 *
 * NOTE: support for daylight savings time is currently very bogus.
 * It's probably best to do without, unless you live in North America.
 *
 */
#define TZNAMLEN	8	/* max. length of time zone name */

time_t 
__tzoffset(char *s, int *hasdst)
{
        time_t off;
        int x, sgn = 1;
	static char stdname[TZNAMLEN+1], dstname[TZNAMLEN+1];
	static char unknwn[4] = "???";

	char *n;

        *hasdst = -1;                   /* Assume unknown */
        if (!s || !*s)
                return 0;               /* Assume GMT */
 
       *hasdst = 0;

	n = stdname;
        while (*s && isalpha(*s)) {
		*n++ = *s++;        /* skip name */
	}
	*n = 0;

/* now figure out the offset */

        x = 0;
        if (*s == '-') {
                sgn = -1;
                s++;
        }
        while (isdigit(*s)) {
                x = 10 * x + toint(*s);
                s++;
        }
        off = x * SECS_PER_HOUR;
        if (*s == ':') {
                x = 0;
                s++;
                while (isdigit(*s)) {
                        x = 10 * x + toint(*s);
                        s++;
                }
	        off += (x * SECS_PER_MIN);
        }

	n = dstname;
        if (isalpha(*s)) {
                *hasdst = 1;
		while (*s && isalpha(*s)) *n++ = *s++;
	}
	*n = 0;

	if (stdname[0])
		_tzname[0] = stdname;
	else
		_tzname[0] = unknwn;

	if (dstname[0])
		_tzname[1] = dstname;
	else
		_tzname[1] = stdname;

        return sgn * off;
}

/*
 * Given a tm struct representing the local time, determine whether
 * DST is currently in effect. This should only be
 * called if it is known that the time zone indeed supports DST.
 *
 * FIXME: For now, assume that everyone follows the North American
 *   time zone rules, all the time. This means daylight savings
 *   time is assumed to be in effect from the second Sunday in March
 *   to the first Sunday in November.
 *
 */

int __indst(const struct tm *t)
{
        if (t->tm_mon == 2) {           /* March */
	  /* see if two sundays have happened yet */
                if (7 + t->tm_wday - t->tm_mday < 0)
                        return 1;       /* yep */
                return 0;
        }
        if (t->tm_mon == 10) {           /* November */
	  /* has sunday happened yet? */
                if (t->tm_wday - t->tm_mday < 0)
                        return 0;       /* yep */
                return 1;
        }
/* Otherwise, see if it's a month between March and November exclusive */
        return (t->tm_mon > 2 && t->tm_mon < 10);
}


#ifdef __GNUC__
/*
 * provide weak aliases (so the user can override) for gmtime_r and localtime_r
 */

struct tm *gmtime_r(const time_t *t, struct tm *stm) __attribute__((weak,alias("_gmtime_r")));
struct tm *localtime_r(const time_t *t, struct tm *stm) __attribute__((weak,alias("_localtime_r")));
#endif
