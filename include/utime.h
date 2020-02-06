#ifndef _UTIME_H
#define _UTIME_H

struct utimbuf {
    time_t actime;   // access time
    time_t modtime;  // access time
};

#define utime(fname, utimbuf) (-1)
#endif
