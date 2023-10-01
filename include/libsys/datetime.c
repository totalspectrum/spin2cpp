//
// date/time functions
//
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

// get the date in the form YYYY-MM-DD
char *_basic_date() {
    char *buf = _gc_alloc(16);
    time_t now = time(NULL);
    struct tm *nowtm = localtime(&now);
    
    strftime(buf, 15, "%F", nowtm);
    return buf;
}

// get the time in 24 hour format
char *_basic_time() {
    char *buf = _gc_alloc(16);
    time_t now = time(NULL);
    struct tm *nowtm = localtime(&now);

    strftime(buf, 15, "%T", nowtm);
    return buf;
}

// set both date and time
time_t _basic_settime(const char *timestr) {
    struct tm nowtm;
    int year, month, day;
    int hour, minute, second;
    time_t newtime;
    
    sscanf(timestr, "%d-%d-%d %i:%i:%i", &year, &month, &day, &hour, &minute, &second);

//    __builtin_printf("settime(%s): read year=%d month=%d\n", timestr, year, month);
    
    // make year relative to 1900
    if (year > 100) {
        year = year - 1900;
    } else {
        year = year + 100;
    }
    nowtm.tm_sec = second;
    nowtm.tm_min = minute;
    nowtm.tm_hour = hour;
    nowtm.tm_mday = day;
    nowtm.tm_mon = month-1;
    nowtm.tm_year = year;

    newtime = mktime(&nowtm);

    struct timeval tv;

    tv.tv_sec = newtime;
    tv.tv_usec = 0;

    settimeofday(&tv, 0);
}
