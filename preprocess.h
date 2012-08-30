#ifndef PREPROCESS_H_
#define PREPROCESS_H_

#include <string.h>
#include "flexbuf.h"

struct predef {
    struct predef *next;
    const char *name;
    const char *def;
};

#define MODE_UNKNOWN 0
#define MODE_UTF8    1
#define MODE_UTF16   2

struct preprocess {
    FILE *f;
    struct flexbuf line;
    struct flexbuf whole;
    int (*readfunc)(struct preprocess *p, char *buf);
    struct predef *defs;
    int ifdepth;
    int skipdepth;
};

#endif
