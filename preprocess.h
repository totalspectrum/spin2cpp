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

    /* comment handling code */
    const char *linecomment;
    const char *startcomment;
    const char *endcomment;

    int incomment;

};

void pp_init(struct preprocess *pp, FILE *f);
void pp_setcomments(struct preprocess *pp, const char *s, const char *e);
char *pp_run(struct preprocess *pp);

#endif
