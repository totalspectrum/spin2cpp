#ifndef PREPROCESS_H_
#define PREPROCESS_H_

#include <string.h>
#include "flexbuf.h"

struct predef {
    struct predef *next;
    const char *name;
    const char *def;
    int  flags;
};
#define PREDEF_FLAG_FREEDEFS 0x01  /* if "name" and "def" should be freed */


#define MODE_UNKNOWN 0
#define MODE_UTF8    1
#define MODE_UTF16   2

struct filestate {
    struct filestate *next;
    FILE *f;
    int lineno;
    int (*readfunc)(FILE *f, char *buf);
    int flags;
};
#define FILE_FLAGS_CLOSEFILE 0x01

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

    /* error handling code */
    void (*errfunc)(void *arg, const char *str, ...);
    void (*warnfunc)(void *arg, const char *str, ...);
    void *errarg;
    void *warnarg;
};

/* initialize for reading */
void pp_init(struct preprocess *pp);

/* push an opened FILE struct */
void pp_push_file(struct preprocess *pp, FILE *f);

/* push a file by name */
void pp_push_name(struct preprocess *pp, const char *filename);

/* pop a file (finish processing it) */
void pp_pop_file(struct preprocess *pp);

/* set the strings that will be recognized to start and end multi-line
   comments; these nest */
void pp_setcomments(struct preprocess *pp, const char *s, const char *e);

/* define symbol "name" to have "val", or undefine it if val is NULL */
void pp_define(struct preprocess *pp, const char *name, const char *val);

/* get the current state of the define stack */
void *pp_get_define_state(struct preprocess *pp);

/* restore the define state to the state given by a previous call to get_define_state */
void pp_restore_define_state(struct preprocess *pp, void *ptr);

/* actually perform the preprocessing on all files that have been pushed so far */
void pp_run(struct preprocess *pp);

/* finish preprocessing and retrieve the result string */
char *pp_finish(struct preprocess *pp);

#endif
