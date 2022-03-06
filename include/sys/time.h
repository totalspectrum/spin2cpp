#ifndef _SYS_TIME_H
#define _SYS_TIME_H

#include <time.h>

typedef unsigned int suseconds_t;

struct timeval {
  time_t tv_sec;  /* seconds (ignoring leapseconds) since the epoch */
  suseconds_t tv_usec;
};

int gettimeofday(struct timeval *tv, void *unused) _IMPL("libc/time/gettimeofday.c");
int settimeofday(const struct timeval *tv, const void *unused) _IMPL("libc/time/gettimeofday.c");

#endif
