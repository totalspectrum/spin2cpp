/*
 * Generic preprocessor
 * Reads UTF-16LE or UTF-8 encoded files, and returns a
 * string containing UTF-8 characters
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "flexbuf.h"
#include "preprocess.h"

/*
 * function to read a single UTF-8 character
 * from a file
 * returns number of bytes placed in buffer, or -1 on EOF
 */
static int
read_single(struct preprocess *pp, char buf[4])
{
    int c = fgetc(pp->f);
    if (c == EOF)
        return -1;
    buf[0] = c;
    return 1;
}

/*
 * function to read a single UTF-16 character
 * from a file
 * returns number of bytes placed in buffer, or -1 on EOF
 */
static int
read_utf16(struct preprocess *pp, char buf[4])
{
    int c, d;
    int r;
    c = fgetc(pp->f);
    if (c < 0)
        return -1;
    d = fgetc(pp->f);
    if (d < 0)
        return -1;

    c = c + (d<<8);
    /* here we need to translate UTF-16 to UTF-8 */
    /* FIXME: this code is not done properly; it does
       not handle surrogate pairs (0xD800 - 0xDFFF)
     */
    if (c < 128) {
        buf[0] = c;
        r = 1;
    } else if (c < 0x800) {
        buf[0] = 0xC0 + ((c>>6) &  0x1F);
        buf[1] = 0x80 + ( c & 0x3F );
        r = 2;
    } else {
        buf[0] = 0xE0 + ((c>>12) & 0x0F);
        buf[1] = 0x80 + ((c>>6) & 0x3F);
        buf[2] = 0x80 + (c & 0x3F);
        r = 3;
    }
    return r;
}

/*
 * read a line
 * returns number of bytes read, or 0 on EOF
 */
int
pp_nextline(struct preprocess *pp)
{
    int r;
    int count = 0;
    FILE *f = pp->f;
    char buf[4];

    flexbuf_clear(&pp->line);
    if (pp->readfunc == NULL) {
        int c0, c1;
        c0 = fgetc(f);
        if (c0 < 0) return 0;
        if (c0 != 0xff) {
            pp->readfunc = read_single;
            flexbuf_addchar(&pp->line, c0);
        } else {
            c1 = fgetc(f);
            if (c1 == 0xfe) {
                pp->readfunc = read_utf16;
            } else {
                pp->readfunc = read_single;
                flexbuf_addchar(&pp->line, c0);
                ungetc(c1, f);
            }
        }
        if (c0 == '\n') {
            flexbuf_addchar(&pp->line, 0);
            return 1;
        }
    }
    for(;;) {
        r = (*pp->readfunc)(pp, buf);
        if (r <= 0) break;
        count += r;
        flexbuf_addmem(&pp->line, buf, r);
        if (r == 1 && buf[0] == '\n') break;
    }
    flexbuf_addchar(&pp->line, '\0');
    return count;
}

/*
 * initialize preprocessor
 */
void
pp_init(struct preprocess *pp, FILE *f)
{
    memset(pp, 0, sizeof(*pp));
    pp->f = f;
    pp->readfunc = NULL;
    flexbuf_init(&pp->line, 128);
    flexbuf_init(&pp->whole, 102400);
}

/*
 * add a definition
 */
void
pp_define(struct preprocess *pp, const char *name, const char *str)
{
    struct predef *the;

    the = calloc(sizeof(*the), 1);
    the->name = name;
    the->def = str;
    the->next = pp->defs;
    pp->defs = the;
}

const char *
pp_getdef(struct preprocess *pp, const char *name)
{
    struct predef *X;
    const char *def = NULL;
    X = pp->defs;
    while (X) {
        if (!strcmp(X->name, name)) {
            def = X->def;
            break;
        }
        X = X->next;
    }
    return def;
}

/* structure describing current parse state of a string */
typedef struct parse_state {
    char *str;  /* pointer to start of string */
    char *save; /* pointer to where last parse ended */
    int   c;    /* saved character */
} ParseState;

static void parse_init(ParseState *P, char *s)
{
    P->str = s;
    P->save = NULL;
    P->c = 0;
}

static char *parse_getword(ParseState *P)
{
    char *word, *ptr;
    int state;

    if (P->save) {
        *P->save = P->c;
        ptr = P->save;
    } else {
        ptr = P->str;
    }
    word = ptr;
    if (!*ptr) return ptr;
    if (isspace(*ptr)) {
        while (*ptr && isspace(*ptr)) {
            ptr++;
        }
    } else {
        state = isalnum(*ptr);
        ptr++;
        while (*ptr && isalnum(*ptr) == state && !isspace(*ptr))
            ptr++;
    }
    P->save = ptr;
    P->c = *ptr;
    *ptr = 0;
    return word;
}

static char *parse_restofline(ParseState *P)
{
    char *ptr;
    char *ret;

    if (P->save) {
        *P->save = P->c;
        ptr = P->save;
    } else {
        ptr = P->str;
    }
    ret = ptr;
    while (*ptr && *ptr != '\n')
        ptr++;
    if (*ptr) {
        P->c = *ptr;
        *ptr = 0;
        P->save = ptr;
    } else {
        P->save = NULL;
    }
    P->str = ret;
    return P->str;
}

static void
parse_skipspaces(ParseState *P)
{
    char *ptr;
    if (P->save) {
        *P->save = P->c;
        ptr = P->save;
    } else {
        ptr = P->str;
    }
    while (*ptr && isspace(*ptr))
        ptr++;
    P->str = ptr;
    P->save = NULL;
}

static char *parse_getwordafterspaces(ParseState *P)
{
    parse_skipspaces(P);
    return parse_getword(P);
}


/*
 * expand macros in a line
 */
static int
expand_macros(struct preprocess *pp, char *data)
{
    ParseState P;
    char *word;
    const char *def;
    int len;

    if (pp->skipdepth > 0)
        return 0;

    parse_init(&P, data);
    for(;;) {
        word = parse_getword(&P);
        if (!*word)
            break;
        if (pp->incomment) {
            if (strstr(word, pp->endcomment)) {
                --pp->incomment;
            } else {
                if (strstr(word, pp->startcomment)) {
                    pp->incomment++;
                }
            }
            def = word;
        } else if (isalpha(*word)) {
            def = pp_getdef(pp, word);
            if (!def)
                def = word;
        } else {
            if (pp->startcomment && strstr(word, pp->startcomment)) {
                pp->incomment++;
            }
            def = word;
        }
        flexbuf_addstr(&pp->line, def);
    }
    len = flexbuf_curlen(&pp->line);
    flexbuf_addchar(&pp->line, 0);
    return len;
}

static void
handle_ifdef(struct preprocess *pp, ParseState *P, int invert)
{
    char *word;
    const char *def;

    if (pp->skipdepth > 0) {
        pp->skipdepth++;
        return;
    }
    word = parse_getwordafterspaces(P);
    def = pp_getdef(pp, word);
    if (invert ^ (def != NULL)) {
        /* test passed */
    } else {
        pp->skipdepth++;
    }
}

static void
handle_else(struct preprocess *pp, ParseState *P)
{
    if (pp->skipdepth > 1) {
        return;
    }
    pp->skipdepth = 1 - pp->skipdepth;
}

static void
handle_endif(struct preprocess *pp, ParseState *P)
{
    if (pp->skipdepth > 0) {
        --pp->skipdepth;
        return;
    }
}

static void
handle_error(struct preprocess *pp, ParseState *P)
{
    char *msg;
    if (pp->skipdepth > 0) {
        return;
    }
    msg = parse_restofline(P);
    fprintf(stderr, "#error: %s\n", msg);
}

static void
handle_define(struct preprocess *pp, ParseState *P)
{
    char *def;
    char *name;
    if (pp->skipdepth > 0) {
        return;
    }
    name = parse_getwordafterspaces(P);
    name = strdup(name);

    parse_skipspaces(P);
    def = parse_restofline(P);
    def = strdup(def);
    fprintf(stderr, "define [%s] to [%s]\n", name, def);
    pp_define(pp, name, def);
}

/*
 * expand a line and process any directives
 */
static int
do_line(struct preprocess *pp)
{
    char *data = flexbuf_get(&pp->line);
    char *func;
    int r;

    if (data[0] != '#' || pp->incomment) {
        r = expand_macros(pp, data);
    } else {
        ParseState P;
        parse_init(&P, data+1);
        parse_skipspaces(&P);
        func = parse_getword(&P);
        if (!strcmp(func, "ifdef")) {
            handle_ifdef(pp, &P, 0);
        } else if (!strcmp(func, "ifndef")) {
            handle_ifdef(pp, &P, 1);
        } else if (!strcmp(func, "else")) {
            handle_else(pp, &P);
        } else if (!strcmp(func, "endif")) {
            handle_endif(pp, &P);
        } else if (!strcmp(func, "error")) {
            handle_error(pp, &P);
        } else if (!strcmp(func, "define")) {
            handle_define(pp, &P);
        } else {
            fprintf(stderr, "Unknown preprocessor directive `%s'\n", func);
        }
        r = 0;
    }
    free(data);
    return r;
}

/*
 * main function
 */
char *
pp_run(struct preprocess *pp)
{
    int linelen;

    for(;;) {
        linelen = pp_nextline(pp);
        if (linelen <= 0) break;  /* end of file */
        /* now expand directives and/or macros */
        linelen = do_line(pp);
        /* if the whole line should be skipped check_directives will return 0 */
        if (linelen == 0) {
            /* add a newline so line number errors will be correct */
            flexbuf_addchar(&pp->whole, '\n');
        } else {
            char *line = flexbuf_get(&pp->line);
            flexbuf_addstr(&pp->whole, line);
        }
    }
    flexbuf_addchar(&pp->whole, 0);
    return flexbuf_get(&pp->whole);
}

/*
 * set comment characters
 */
void
pp_setcomments(struct preprocess *pp, const char *start, const char *end)
{
    pp->startcomment = start;
    pp->endcomment = end;
}

char *
preprocess(const char *filename)
{
    struct preprocess pp;
    FILE *f;
    char *result;

    f = fopen(filename, "rb");
    if (!f) {
        perror(filename);
        return NULL;
    }
    pp_init(&pp, f);
    result = pp_run(&pp);
    fclose(f);
    return result;
}

#ifdef TEST
int
main(int argc, char **argv)
{
    char *str;
    if (argc != 2) {
        fprintf(stderr, "usage: %s file\n", argv[0]);
        return 2;
    }
    str = preprocess(argv[1]);
    if (!str) {
        fprintf(stderr, "error reading file %s\n", argv[1]);
        return 1;
    } else {
        printf("%s", str);
        fflush(stdout);
    }
    return 0;
}
#endif
