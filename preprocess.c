/*
 * Generic and very simple preprocessor
 * Copyright (c) 2012-2023 Total Spectrum Software Inc.
 * MIT Licensed, see terms of use at end of file
 *
 * Reads UTF-16LE or UTF-8 encoded files, and returns a
 * string containing UTF-8 characters.
 * The following directives are supported:
 *  #define FOO  - simple macros with no arguments
 *  #undef
 *  #ifdef FOO / #ifndef FOO
 *  #else / #elseifdef FOO / #elseifndef FOO
 *  #endif
 *  #error message
 *  #warn message
 *  #include "file"
 *
 * Any other # directives are passed through.
 *
 * Also, code wrapped in a comment that starts:
 * {$flexspin
 * $}
 * (both need to be the only thing on the line) will be passed through
 * to flexspin. This happens even before preprocessing.
 *
 * Note that the preprocessor itself will insert #line directives
 * before and after any included text, of the form:
 *   #line NNNN "filename"
 * where NNNN is a decimal line number. The scanner or parser may
 * use these directives to make sure error numbers come out right.
 *
 * Here's an example of reading a file foo.txt in and preprocessing
 * it in an environment where "VALUE1" is defined to "VALUE" and
 * "VALUE2" is defined to "0":
 *
 *   struct preprocess pp;
 *   char *parser_str;
 *
 *   pp_init(&pp);
 *   pp_define(&pp, "VALUE1", "VALUE");
 *   pp_define(&pp, "VALUE2", "0");
 *   pp_push_file(&pp, "foo.txt");
 *   pp_run(&pp);
 *   // any additional files to read can be pushed and run here
 *   parser_str = pp_finish(&pp);
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdarg.h>
#include <sys/stat.h>
#include "preprocess.h"
#include "util/util.h"

#define LINENO(A) ( ((A)->lineno) )

// hook for keeping track of source files
extern void AddSourceFile(const char *shortName, const char *fullName);

#ifdef _MSC_VER
# ifndef strdup
#define strdup _strdup
# endif
#endif

// utility function to open a regular file (only), ignoring directories
FILE *
fopen_fileonly(const char *name, const char *mode) {
    FILE *f;
    struct stat sbuf;
    int r;

    r = stat(name, &sbuf);
    if (r) return NULL;
    if (sbuf.st_mode & S_IFDIR) {
        return NULL;
    }
    f = fopen(name, mode);
    return f;
}

// forward declaration
static void doerror(struct preprocess *pp, const char *msg, ...);

/*
 * function to read a single LATIN-1 character
 * from a file
 * returns number of bytes placed in buffer, or -1 on EOF
 */
static int
read_latin1(FILE *f, char buf[4])
{
  int c = fgetc(f);
  if (c == EOF)
    return -1;
  if (c <= 127) {
    buf[0] = (char)c;
    return 1;
  }
  buf[0] = 0xC0 + ((c>>6) & 0x1f);
  buf[1] = 0x80 + ( c & 0x3f );
  return 2;
}

/*
 * function to read a single UTF-8 character
 * from a file
 * returns number of bytes placed in buffer, or -1 on EOF
 */
static int
read_single(FILE *f, char buf[4])
{
    int c = fgetc(f);
    if (c == EOF)
        return -1;
    buf[0] = (char)c;
    return 1;
}

/*
 * function to read a single UTF-16 character
 * from a file
 * returns number of bytes placed in buffer, or -1 on EOF
 */
static int
read_utf16(FILE *f, char buf[4])
{
    int c, d;
    int r;
    c = fgetc(f);
    if (c < 0)
        return -1;
    d = fgetc(f);
    if (d < 0)
        return -1;

    c = c + (d<<8);
    /* here we need to translate UTF-16 to UTF-8 */
    /* FIXME: this code is not done properly; it does
       not handle surrogate pairs (0xD800 - 0xDFFF)
     */
    if (c < 128) {
        buf[0] = (char)c;
        r = 1;
    } else if (c < 0x800) {
        buf[0] = 0xC0 + ((c>>6) &  0x1F);
        buf[1] = 0x80 + ( c & 0x3F );
        r = 2;
    } else if (c < 0x10000) {
        buf[0] = 0xE0 + ((c>>12) & 0x0F);
        buf[1] = 0x80 + ((c>>6) & 0x3F);
        buf[2] = 0x80 + (c & 0x3F);
        r = 3;
    } else {
        buf[0] = 0xE0 + ((c>>18) & 0x07);
        buf[1] = 0x80 + ((c>>12) & 0x3F);
        buf[2] = 0x80 + ((c>>6) & 0x3F);
        buf[3] = 0x80 + (c & 0x3F);
        r = 4;
    }
    return r;
}


/*
 * read a line
 * returns number of bytes read, or 0 on EOF
 *
 * here at the lowest level we also transform some
 * special comment sequences:
 *
 * {$flexspin
 * ...
 * }
 *
 * is actually turned from a comment into plain code
 * NOTE: the closing } must be the first thing on the line!
 */
int
pp_nextline(struct preprocess *pp)
{
    int r;
    int count = 0;
    FILE *f;
    char buf[4];
    char *full_line;
    struct filestate *A;

    A = pp->fil;
    if (!A)
        return 0;
    f = A->f;

    flexbuf_clear(&pp->line);
    if (A->readfunc == NULL) {
        int c0, c1;
        c0 = fgetc(f);
        if (c0 < 0) return 0;
	if (c0 == 0xef) {
	    A->readfunc = read_single;
	    c1 = fgetc(f);
	    if (c1 == 0xbb) {
	        c1 = fgetc(f);
		if (c1 == 0xbf) {
		  /* discard the byte order mark */
		} else {
		  ungetc(c1, f);
		  ungetc(0xbb, f);
		}
	    } else {
	        ungetc(c1, f);
	    }
	} else
        if (c0 != 0xff) {
            if (c0 >= 0x80 && c0 < 0xc0) {
	        A->readfunc = read_latin1;
	    } else {
                A->readfunc = read_single;
	    }
            flexbuf_addchar(&pp->line, c0);
        } else {
            c1 = fgetc(f);
            if (c1 == 0xfe) {
                A->readfunc = read_utf16;
            } else {
                A->readfunc = read_single;
                flexbuf_addchar(&pp->line, c0);
                ungetc(c1, f);
            }
        }
        A->skipnl = 0;
        if (c0 == '\n') {
            flexbuf_addchar(&pp->line, 0);
            A->lineno++;
            return 1;
        }
    }
    for(;;) {
        r = (*A->readfunc)(f, buf);
        if (r <= 0) break;
        if (buf[0] == '\n' && A->skipnl) {
            A->skipnl = 0;
            continue;
        }
        if (buf[0] == '\r') {
            buf[0] = '\n';
            A->skipnl = 1; // if CR NL is seen, skip the NL
        }
        count += r;
        flexbuf_addmem(&pp->line, buf, r);
        if (buf[0] == '\n') {
            A->lineno++;
            break;
        } else {
            A->skipnl = 0;
        }
    }
    flexbuf_addchar(&pp->line, '\0');
    if (pp->incomment == 0) {
        /* look for special sequences */
        full_line = flexbuf_peek(&pp->line);
        if (   !strncmp(full_line, "{$flexspin", 10)
            || !strncmp(full_line, "{$preproc", 9) )
        {
            char *new_line;
            if (pp->special_flexspin_comment) {
                doerror(pp, "cannot nest {$flexspin or {$preproc comments");
            }
            pp->special_flexspin_comment++;
            full_line = flexbuf_get(&pp->line);
            new_line = full_line + 10;
            count -= 10;
            while (*new_line == ' ') {
                new_line++;
                count--;
            }
            flexbuf_addmem(&pp->line, new_line, count+1);
            free(full_line);
            full_line = flexbuf_peek(&pp->line);
        }
        if (pp->special_flexspin_comment) {
            char *s;
            /* search for closing '}' */
            for (s = full_line; *s; s++) {
                if (*s == '{') {
                    pp->incomment++;
                } else if (*s == '}') {
                    if (pp->incomment > 0) {
                        --pp->incomment;
                    } else {
                        // remove that one character
                        int prefix_len = s - full_line;
                        full_line = flexbuf_get(&pp->line);
                        if (prefix_len) {
                            flexbuf_addmem(&pp->line, full_line, prefix_len);
                        }
                        flexbuf_addmem(&pp->line, full_line + prefix_len + 1, count - prefix_len);
                        --count;
                        pp->special_flexspin_comment = 0;
                    }
                }
            }
        }           
    }
    return count;
}

/*
 * default error handling functions
 */
static void default_errfunc(void *dummy, const char *filename, int line, const char *msg)
{
    const char *level = (const char *)dummy;
    extern int gl_errors;
    
    if (!strcmp(level, "error")) {
        gl_errors++;
    }
    fprintf(stderr, "%s:%d: %s: ", filename, line, level);
    fprintf(stderr, "%s", msg);
    fprintf(stderr, "\n");
}

static void
doerror(struct preprocess *pp, const char *msg, ...)
{
    va_list args;
    char tmpmsg[BUFSIZ];
    struct filestate *fil;

    va_start(args, msg);
    vsnprintf(tmpmsg, sizeof(tmpmsg), msg, args);
    va_end(args);

    pp->numerrors++;
    fil = pp->fil;
    if (fil) {
        (*pp->errfunc)(pp->errarg, pp->fil->name, pp->fil->lineno, tmpmsg);
    } else {
        (*pp->errfunc)(pp->errarg, "", 0, tmpmsg);
    }
}

static void
dowarning(struct preprocess *pp, const char *msg, ...)
{
    va_list args;
    char tmpmsg[BUFSIZ];
    struct filestate *fil;

    pp->numwarnings++;
    va_start(args, msg);
    vsnprintf(tmpmsg, sizeof(tmpmsg), msg, args);
    va_end(args);

    fil = pp->fil;
    if (fil) {
        (*pp->warnfunc)(pp->warnarg, pp->fil->name, pp->fil->lineno, tmpmsg);
    } else {
        (*pp->warnfunc)(pp->warnarg, "", 0, tmpmsg);
    }
}

/*
 * initialize preprocessor
 */
void
pp_init(struct preprocess *pp)
{
    memset(pp, 0, sizeof(*pp));
    flexbuf_init(&pp->line, 128);
    flexbuf_init(&pp->whole, 102400);
    flexbuf_init(&pp->inc_path, 128);
    
    pp->errfunc = default_errfunc;
    pp->warnfunc = default_errfunc;
    pp->errarg = (void *)"error";
    pp->warnarg = (void *)"warning";
    pp->linechange = "#line %d \"%s\"\n";
}

/*
 * push a file into the preprocessor
 * files will be processed in LIFO order,
 * so the one on top of the stack is the
 * "current" one; this makes #include implementation
 * easier
 */
void
pp_push_file_struct(struct preprocess *pp, FILE *f, const char *filename)
{
    struct filestate *A;

    A = (struct filestate *)calloc(1, sizeof(*A));
    if (!A) {
        doerror(pp, "Out of memory!\n");
        return;
    }
    A->lineno = 1;
    A->f = f;
    A->next = pp->fil;
    A->name = filename;
    pp->fil = A;
    if (A->name) {
        size_t tempsiz = 128 + strlen(A->name);
        char *temp = (char *)alloca(tempsiz);
        snprintf(temp, tempsiz, pp->linechange, LINENO(A), A->name);
        temp[tempsiz-1] = 0; /* make sure it is 0 terminated */
        flexbuf_addstr(&pp->whole, temp);
    }
}

void
pp_push_file(struct preprocess *pp, const char *name)
{
    FILE *f;

    f = fopen_fileonly(name, "rb");
    if (!f) {
        doerror(pp, "Unable to open file %s", name);
        return;
    }
    pp_push_file_struct(pp, f, name);
    pp->fil->flags |= FILE_FLAGS_CLOSEFILE;
}

/*
 * pop the current file state off the stack
 * closes the file as a side effect
 */
void pp_pop_file(struct preprocess *pp)
{
    struct filestate *A;
    struct ifstate *I;

    while (pp->ifs) {
        I = pp->ifs;
        if (I->fil != pp->fil) {
            break;
        }
        pp->ifs = I->next;
        doerror(pp, "Unterminated #if starting at line %d", I->linenum);
        free(I);
    }

    A = pp->fil;
    if (A) {
        pp->fil = A->next;
        if (A->flags & FILE_FLAGS_CLOSEFILE)
            fclose(A->f);
        free(A);
        A = pp->fil;
        if (A && A->name) {
            char temp[128];
            temp[0] = '\n'; // force a newline
            snprintf(temp+1, sizeof(temp)-1, pp->linechange, LINENO(A), A->name);
            temp[127] = 0; /* make sure it is 0 terminated */
            flexbuf_addstr(&pp->whole, temp);
        }
    }
    pp->incomment = 0;
}

/*
 * add a definition
 * "flags" indicates things like whether we must free the memory
 * associated with name and def
 */
static void
pp_define_internal(struct preprocess *pp, const char *name, const char *def, int flags)
{
    struct predef *the;

    the = (struct predef *)calloc(sizeof(*the), 1);
    the->name = name;
    the->def = def;
    the->flags = flags;
    the->next = pp->defs;
    pp->defs = the;
}

/*
 * the user visible "pp_define"; used mainly for constant strings and
 * such, so we do not free those
 */
void
pp_define(struct preprocess *pp, const char *name, const char *str)
{
    pp_define_internal(pp, name, str, 0);
}

/*
 * retrieve a definition
 * returns NULL if no definition exists (or if there was an
 * explicit #undef)
 */
const char *
pp_getdef(struct preprocess *pp, const char *name)
{
    struct predef *X;
    const char *def = NULL;
    int (*strcmp_func)(const char *a, const char *b);
    
    X = pp->defs;
    if (pp->ignore_case) {
        strcmp_func = strcasecmp;
    } else {
        strcmp_func = strcmp;
    }
    while (X) {
        if (!strcmp_func(X->name, name)) {
            def = X->def;
            break;
        }
        X = X->next;
    }
    if (!def) {
        static char newdef[1024];
        // check for certain predefined words
        if (!strcmp_func("__FILE__", name)) {
            snprintf(newdef, sizeof(newdef), "\"%s\"", pp->fil->name);
            def = newdef;
        } else if (!strcmp_func("__LINE__", name)) {
            snprintf(newdef, sizeof(newdef), "%d", pp->fil->lineno - 1);
            def = newdef;
        }
    }
    return def;
}

/* structure describing current parse state of a string */
typedef struct parse_state {
    char *str;  /* pointer to start of string */
    char *save; /* pointer to where last parse ended */
    int   c;    /* saved character */
    struct preprocess *preproc;
} ParseState;

static void parse_init(ParseState *P, char *s, struct preprocess *pp)
{
    P->str = s;
    P->save = NULL;
    P->c = 0;
    P->preproc = pp;
}

#define PARSE_ISSPACE 1
#define PARSE_IDCHAR  2
#define PARSE_OTHER   3
#define PARSE_QUOTE   4
#define PARSE_COMMENT 5

static int
classify_char(struct preprocess *pp, int c)
{
    if (isspace(c))
        return PARSE_ISSPACE;
    if (isalnum(c) || (c == '_'))
        return PARSE_IDCHAR;
    if (c == '"')
        return PARSE_QUOTE;
    if (strchr(pp->startcomment, c) || strchr(pp->endcomment, c)) {
        return PARSE_COMMENT;
    }
    return PARSE_OTHER;
}

/*
 * skip over a quoted string
 * returns a pointer to the last character (normally a quote)
 */
static char *
skip_quoted_string(char *ptr, int lastchar)
{
  while (*ptr && (*ptr != lastchar))
    ptr++;
  return ptr;
}

/*
 * fetch the next word
 * a word is a sequence of identifier characters, spaces, or
 * other characters
 */
static char *parse_getword(ParseState *P)
{
    char *word, *ptr;
    int state;
    struct preprocess *pp = P->preproc;
    
    if (P->save) {
        *P->save = (char)P->c;
        ptr = P->save;
    } else {
        ptr = P->str;
    }
    word = ptr;
    if (!*ptr) return ptr;

    if (*ptr == '\"') {
        ptr = skip_quoted_string(ptr+1, '\"');
        if (*ptr == '\"') ptr++;
    } else if (*ptr == pp->startcomment[0] && !strncmp(ptr, pp->startcomment, strlen(pp->startcomment))) {
        ptr += strlen(pp->startcomment);        
    } else if (*ptr == pp->endcomment[0] && !strncmp(ptr, pp->endcomment, strlen(pp->endcomment))) {
        ptr += strlen(pp->endcomment);
    } else {
        state = classify_char(pp, (unsigned char)*ptr);
        ptr++;
        while (*ptr && (classify_char(pp, (unsigned char)*ptr) == state))
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
      *P->save = (char)(P->c);
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

static char *parse_getincludestring(ParseState *P)
{
    char *ptr, *start;
    char endchar;
    parse_skipspaces(P);
    ptr = P->str;
    if (*ptr == '\"') {
        endchar = '\"';
    } else if (*ptr == '<') {
        endchar = '>';
    } else {
        return NULL;
    }
    ptr++;
    start = ptr;
    ptr = skip_quoted_string(ptr, endchar);
    if (!*ptr)
        return NULL;
    P->save = ptr;
    P->c = *ptr;
    *ptr = 0;
    return start;
}


/*
 * expand macros and clean comments in a buffer
 * "src" is the source data
 * "dst" is a destination flexbuf
 * "doMacros" is the number of times we should expand macros
 *    normally this starts at 2, but can be 0 if no expansion is desired
 */
static int
do_expand(struct preprocess *pp, struct flexbuf *dst, char *src, int doMacros)
{
    ParseState P;
    char *word;
    const char *def;
    int len;
    bool in_linecomment = false;
    
    if (!pp_active(pp))
        return 0;

    parse_init(&P, src, pp);
    for(;;) {
        word = parse_getword(&P);
        if (!*word)
            break;
        if (pp->incomment) {
            if (in_linecomment) {
                // do nothing with regular comments
            } else if (strstr(word, pp->endcomment)) {
                --pp->incomment;
            } else {
                if (strstr(word, pp->startcomment)) {
                    pp->incomment++;
                }
            }
            def = word;
        } else if (isalpha((unsigned char)*word) || *word == '_') {
            if (doMacros) {
                def = pp_getdef(pp, word);
                if (!def)
                    def = word;
            } else {
                def = word;
            }
        } else {
            if (pp->linecomment && !strncmp(word, pp->linecomment, strlen(pp->linecomment))) {
                pp->incomment++;
                in_linecomment = true;
            } else if (pp->startcomment && strstr(word, pp->startcomment)) {
                pp->incomment++;
            }
            def = word;
        }
        flexbuf_addstr(dst, def);
        if (in_linecomment && strchr(def, '\n')) {
            pp->incomment--;
            in_linecomment = false;
        }
    }
    len = flexbuf_curlen(dst);
    flexbuf_addchar(dst, 0);
    if (doMacros > 1) {
        src = flexbuf_get(dst);
        len = do_expand(pp, dst, src, doMacros-1);
    }
    return len;
}

static int
expand_macros(struct preprocess *pp, struct flexbuf *dst, char *src)
{
    return do_expand(pp, dst, src, 2);
}

static void
handle_ifdef(struct preprocess *pp, ParseState *P, int invert)
{
    char *word;
    const char *def;
    struct ifstate *I;
    int live = pp_active(pp);
    
    I = (struct ifstate *)calloc(1, sizeof(*I));
    if (!I) {
        doerror(pp, "Out of memory\n");
        return;
    }
    I->next = pp->ifs;
    I->fil = pp->fil;
    if (pp->fil) {
        I->linenum = pp->fil->lineno;
    }
    pp->ifs = I;
    if (!live) {
        I->skip = 1;
        I->skiprest = 1;  /* skip all branches, even else */
        return;
    }
    word = parse_getwordafterspaces(P);
    def = pp_getdef(pp, word);
    if (invert ^ (def != NULL)) {
        I->skip = 0;
        I->skiprest = 1;
    } else {
        I->skip = 1;
    }
}

static void
handle_else(struct preprocess *pp, ParseState *P)
{
    struct ifstate *I = pp->ifs;

    (void)(P); /* unused parameter */
    if (!I) {
        doerror(pp, "#else without matching #ifdef");
        return;
    }
    if (I->sawelse) {
        doerror(pp, "multiple #else statements in #ifdef");
        return;
    }
    I->sawelse = 1;
    if (I->skiprest) {
        /* some branch was already handled */
        I->skip = 1;
    } else {
        I->skip = 0;
        I->skiprest = 1;
    }
}

static void
handle_elseifdef(struct preprocess *pp, ParseState *P, int invert)
{
    struct ifstate *I = pp->ifs;
    char *word;
    const char *def;

    if (!I) {
        doerror(pp, "#else without matching #ifdef");
        return;
    }

    if (I->skiprest) {
        /* some branch was already handled */
        I->skip = 1;
        return;
    }
    word = parse_getwordafterspaces(P);
    def = pp_getdef(pp, word);
    if (invert ^ (def != NULL)) {
        I->skip = 0;
        I->skiprest = 1;
    } else {
        I->skip = 1;
    }
}

static void
handle_endif(struct preprocess *pp, ParseState *P)
{
    struct ifstate *I = pp->ifs;

    if (!I) {
        doerror(pp, "#endif without matching #ifdef");
        return;
    }
    pp->ifs = I->next;
    free(I);
}

static void
handle_error(struct preprocess *pp, ParseState *P)
{
    char *msg;
    if (!pp_active(pp)) {
        return;
    }
    msg = parse_restofline(P);
    doerror(pp, "#error: %s", msg);
}

static void
handle_warn(struct preprocess *pp, ParseState *P)
{
    char *msg;
    if (!pp_active(pp)) {
        return;
    }
    msg = parse_restofline(P);
    dowarning(pp, "#warn: %s", msg);
}

static void
handle_define(struct preprocess *pp, ParseState *P, int isDef)
{
    char *def;
    char *name;
    const char *oldvalue;
    struct flexbuf newdef;

    if (!pp_active(pp)) {
        return;
    }
    name = parse_getwordafterspaces(P);
    if (classify_char(pp, name[0]) != PARSE_IDCHAR) {
        doerror(pp, "%s is not a valid identifier for define", name);
        return;
    }
    oldvalue = pp_getdef(pp, name);
    if (oldvalue && isDef) {
        dowarning(pp, "redefining `%s'", name);
    }
    name = strdup(name);

    if (isDef) {
        parse_skipspaces(P);
        def = parse_restofline(P);
        flexbuf_init(&newdef, 80);
        do_expand(pp, &newdef, def, 0);
        def = flexbuf_get(&newdef);
    } else {
        def = NULL;
    }
    pp_define_internal(pp, name, def, PREDEF_FLAG_FREEDEFS);
}

//
// handle export_def pragma:
//  exportdef(NAME) exports the macro NAME to the global namespace
//
static void
handle_export_def(struct preprocess *pp, ParseState *P)
{
    int nest = 0;
    bool more = true;
    char *word;
    
    while (more) {
        word = parse_getwordafterspaces(P);
        //printf("word=[%s]\n", word);
        if (!strcmp(word, "(")) {
            nest++;
        } else if (!strcmp(word, ")")) {
            --nest;
            if (nest <= 0) more = false;
        } else if (!strcmp(word, ",")) {
            // do nothing
        } else if (isalpha(word[0]) || word[0] == '_') {
            const char *def = pp_getdef(pp, word);
            if (def) {
                pp_define_weak_global(pp, strdup(word), strdup(def));
            } else {
                doerror(pp, "exportdef: request to export undefined macro `%s'", word);
            }
        } else if (!word[0]) {
            more = false;
        } else {
            doerror(pp, "Unexpected token in exportdef: `%s'", word);
        }
    }
}

//
// process a pragma
// returns true if pragma handled, false if not
//
static bool
handle_pragma(struct preprocess *pp, ParseState *P)
{
    char *word;
    int live = pp_active(pp);

    if (!live) {
        // not live, just consume the pragma and ignore it
        return true;
    }
    word = parse_getwordafterspaces(P);
    if (!strcmp(word, "ignore_case")) {
        pp->ignore_case = 1;
        return true;
    } else if (!strcmp(word, "keep_case")) {
        pp->ignore_case = 0;
        return true;
    } else if (!strcmp(word, "exportdef")) {
        handle_export_def(pp, P);
        return true;
    }
    return false;
}

void
pp_add_to_path(struct preprocess *pp, const char *dir)
{
    flexbuf_addmem(&pp->inc_path, (const char *)&dir, sizeof(const char **));
}

static char *
find_file_relative(struct preprocess *pp, const char *name, const char *ext, const char *relpath, const char *abspath)
{
  const char *path = NULL;
  char *ret;
  char *last;
  FILE *f;
  int trimname = 1;
  
#ifdef WIN32
  if (isalpha(name[0]) && name[1] == ':') {
      /* absolute path */
      path = "";
  } else
#endif      
  if (name[0] == '/' || name[0] == '\\') {
    /* absolute path */
    path = "";
  } else if (relpath) {
      path = relpath;
  } else if (abspath) {
      path = abspath;
      if (path[0]) trimname = 0;
  } else {
      if (pp && pp->fil) {
          path = pp->fil->name;
          trimname = 1;
      }
      if (!path)
          path = "";
  }
  if (ext) {
      ret = (char *)malloc(strlen(path) + strlen(name) + strlen(ext) + 2);
  } else {
      ret = (char *)malloc(strlen(path) + strlen(name) + 2);
  }
  if (!ret) {
    fprintf(stderr, "FATAL ERROR: out of memory\n");
    exit(2);
  }
  strcpy(ret, path);
#ifdef WIN32
  {
      char *test = ret;
      int c;
      last = 0;
      while ( (c = *test) != 0 ) {
          if (c == '\\') {
              *test = '/';
          }
          test++;
      }
  }
#endif
  last = strrchr(ret, '/');

  if (trimname) {
      if (last) {
          last[1] = 0;
      } else {
          ret[0] = 0;
      }
  } else {
      if (!last || last[1] != 0) {
          strcat(ret, "/");
      }
  }
  strcat(ret, name);
  f = fopen_fileonly(ret, "r");
  if (!f && ext) {
    strcat(ret, ext);
    f = fopen_fileonly(ret, "r");
  }
  //printf("... trying %s\n", ret);
  if (!f) {
    /* give up */
    free(ret);
    ret = NULL;
  }
  if (f)
    fclose(f);
  return ret;
}

char *
find_file_on_path(struct preprocess *pp, const char *name, const char *ext, const char *relpath)
{
    char *r;
    size_t num_abs_paths;
    size_t i;
    const char **abs_paths;

    r = find_file_relative(pp, name, ext, relpath, NULL);
    if (r) return r;
    /* not found yet; look along the include path */
    num_abs_paths = flexbuf_curlen(&pp->inc_path) / sizeof(const char **);
    abs_paths = (const char **)flexbuf_peek(&pp->inc_path);
    for (i = 0; i < num_abs_paths; i++) {
        r = find_file_relative(pp, name, ext, NULL, abs_paths[i]);
        if (r) return r;
    }
    return NULL;
}

static void
handle_include(struct preprocess *pp, ParseState *P)
{
    char *name;
    char *orig_name;

    if (!pp_active(pp)) {
        return;
    }
    orig_name = parse_getincludestring(P);
    if (!orig_name) {
        doerror(pp, "no string found for include");
        return;
    }
    /* if the file does not start with a /, it could be relative
       to the current file name
    */
    name = find_file_on_path(pp, orig_name, NULL, NULL);
    if (!name)
      name = strdup(orig_name);
    pp_push_file(pp, name);
    pp->fil->lineno = 0;  /* hack to correct for \n in buffer?? */
    AddSourceFile(strdup(orig_name), name);
}

/*
 * expand a line and process any directives
 * returns 0 if the line should be skipped, otherwise returns the length
 * of the expanded line
 */
static int
do_line(struct preprocess *pp)
{
    char *data = flexbuf_get(&pp->line);
    char *func;
    int r = 0;

    if (data[0] != '#' || pp->incomment) {
        r = expand_macros(pp, &pp->line, data);
    } else {
        ParseState P;
        parse_init(&P, data+1, pp);
        parse_skipspaces(&P);
        func = parse_getword(&P);
        if (!strcasecmp(func, "ifdef")) {
            handle_ifdef(pp, &P, 0);
        } else if (!strcasecmp(func, "ifndef")) {
            handle_ifdef(pp, &P, 1);
        } else if (!strcasecmp(func, "else")) {
            handle_else(pp, &P);
        } else if (!strcasecmp(func, "elseifdef")) {
            handle_elseifdef(pp, &P, 0);
        } else if (!strcasecmp(func, "elseifndef")) {
            handle_elseifdef(pp, &P, 1);
        } else if (!strcasecmp(func, "endif")) {
            handle_endif(pp, &P);
        } else if (!strcasecmp(func, "error")) {
            handle_error(pp, &P);
        } else if (!strcasecmp(func, "warn")) {
            handle_warn(pp, &P);
        } else if (!strcasecmp(func, "warning")) {
            // obsolete form of #warn
            handle_warn(pp, &P);
        } else if (!strcasecmp(func, "define")) {
            handle_define(pp, &P, 1);
        } else if (!strcasecmp(func, "undef")) {
            handle_define(pp, &P, 0);
        } else if (!strcasecmp(func, "include")) {
            handle_include(pp, &P);
        } else if (!strcasecmp(func, "pragma")) {
            if (!handle_pragma(pp, &P)) {
                // pragma was not recognized, pass it through
                if (P.save) {
                    *P.save = P.c;
                    P.save = NULL;
                }
                r = expand_macros(pp, &pp->line, data);
            }
        } else {
            if (!strcasecmp(func, "line"))
            {
                /* no warning for these directives */
            } else if (isdigit(func[0]) || func[0] == '$' || func[0] == '%') {
                /* could be a Spin constant decl, ignore */
            } else {
                dowarning(pp, "Ignoring unknown preprocessor directive `%s'", func);
            }   
            // just pass it through for the language processor to deal with
            if (P.save) {
                *P.save = P.c;
                P.save = NULL;
            }
            r = expand_macros(pp, &pp->line, data);
        }
    }
    free(data);
    return r;
}

/*
 * main function
 */
void
pp_run(struct preprocess *pp)
{
    int linelen;

    while (pp->fil) {
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
        pp_pop_file(pp);
    }
}

char *
pp_finish(struct preprocess *pp)
{
    flexbuf_addchar(&pp->whole, 0);
    return flexbuf_get(&pp->whole);
}

/*
 * set comment characters
 */
void
pp_setcomments(struct preprocess *pp, const char *line, const char *start, const char *end)
{
    pp->linecomment = line;
    pp->startcomment = start;
    pp->endcomment = end;
}

/*
 * set line directive format
 */
void
pp_setlinedirective(struct preprocess *pp, const char *dir)
{
    pp->linechange = dir;
}

/*
 * get define state into arguments (like -DXXX)
 * returns new argc
 */
int
pp_get_defines_as_args(struct preprocess *pp, int argc, char **argv, int max_argc)
{
    struct predef *x;
    const char *def;
    char *cdef;
    int i, n;
    char **inc_paths;;
    int num_inc_paths;

    /* first, fill out -I args for our paths */
    num_inc_paths = flexbuf_curlen(&pp->inc_path) / sizeof(const char **);
    inc_paths = (char **)flexbuf_peek(&pp->inc_path);
    for (i = 0; i < num_inc_paths; i++) {
        n = strlen(inc_paths[i]) + 4;
        cdef = (char *)malloc(n);
        sprintf(cdef, "-I%s", inc_paths[i]);
        argv[argc++] = cdef;
        if (argc >= max_argc) return argc;
    }
    
    /* now, fill in all definitions */
    x = pp->defs;
    while (x) {
        if (!x->argcdef) {
            n = strlen(x->name);
            if (x->def) {
                def = x->def;
            } else {
                def = "1";
            }
            n += strlen(def);
            n += 4; /* room for "-D", "=", and trailing 0 */
            cdef = (char *)malloc(n);
            sprintf(cdef, "-D%s=%s", x->name, def);
            x->argcdef = cdef;
        }
        argv[argc++] = (char *)x->argcdef;
        if (argc >= max_argc) return argc;
        x = x->next;
    }
    return argc;
}

/*
 * get/restore define state
 * this may be used to ensure that #defines in sub files are not
 * seen in the main file
 */
void *
pp_get_define_state(struct preprocess *pp)
{
    return (void *)pp->defs;
}

void
pp_restore_define_state(struct preprocess *pp, void *vp)
{
    struct predef *where = (struct predef *)vp;
    struct predef *x, *old;

    x = pp->defs;
    while (x && x != where) {
        old = x;
        x = old->next;
        if (old->flags & PREDEF_FLAG_FREEDEFS)
        {
            free((void *)old->name);
            if (old->def) free((void *)old->def);
        }
        free(old);
    }
    pp->defs = x;
}

void
pp_define_weak_global(struct preprocess *pp, const char *name, const char *def)
{
    struct predef *x, *the;
    the = (struct predef *)calloc(sizeof(*the), 1);
    the->name = name;
    the->def = def;
    the->flags = 0;
    the->next = NULL;
    if (!pp->defs) {
        pp->defs = the;
        return;
    }
    x = pp->defs;
    // postpend the definition
    while (x->next) {
        x = x->next;
    }
    x->next = the;
}


/* hook for mcpp */
void mcpp_export_define(const char *ident, const char *def)
{
    pp_define_weak_global(&gl_pp, strdup(ident), strdup(def));
}

#ifdef TESTPP
int gl_errors;

struct preprocess gl_pp;

/* dummy stub */
void AddSourceFile(const char *, const char *) {
}

/* test code */
char *
preprocess(const char *filename)
{
    FILE *f;
    char *result;

    f = fopen(filename, "rb");
    if (!f) {
        perror(filename);
        return NULL;
    }
    pp_init(&gl_pp);
    pp_setcomments(&gl_pp, "'", "{", "}");  // set up for Spin comments
    pp_push_file_struct(&gl_pp, f, filename);
    pp_run(&gl_pp);
    result = pp_finish(&gl_pp);
    fclose(f);
    return result;
}

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

/*
 * +--------------------------------------------------------------------
 * ¦  TERMS OF USE: MIT License
 * +--------------------------------------------------------------------
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * +--------------------------------------------------------------------
 */
