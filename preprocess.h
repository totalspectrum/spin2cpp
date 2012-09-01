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

struct filestate {
    struct filestate *next;
    FILE *f;
    int lineno;
    int (*readfunc)(FILE *f, char *buf);
};

struct preprocess {
    struct filestate *fil;
    struct flexbuf line;
    struct flexbuf whole;
    struct predef *defs;

    int ifdepth;
    int skipdepth;

    /* comment handling code */
    const char *linecomment;
    const char *startcomment;
    const char *endcomment;

    int incomment;

};

void pp_init(struct preprocess *pp);
void pp_push_file(struct preprocess *pp, FILE *f);
void pp_pop_file(struct preprocess *pp);
void pp_setcomments(struct preprocess *pp, const char *s, const char *e);
void pp_define(struct preprocess *pp, const char *name, const char *val);
void *pp_get_define_state(struct preprocess *pp);
void pp_restore_define_state(struct preprocess *pp, void *ptr);
void pp_run(struct preprocess *pp);
char *pp_finish(struct preprocess *pp);

#endif
