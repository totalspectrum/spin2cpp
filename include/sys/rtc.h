/*
 * real time clock support
 */

#ifndef _SYS_RTC_H
#define _SYS_RTC_H

#include <sys/time.h>

int (*_rtc_gettime)(struct timeval *tv);
int (*_rtc_settime)(const struct timeval *tv);

extern void _rtc_start_timekeeping_cog(void);

extern int _default_ticks_updated;
extern void _default_update_ticks(void);

#endif
