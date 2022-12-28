/* Program to extract function declarations from C source code
 * Written by Eric R. Smith and placed in the public domain
 * Thanks to:
 * Jwahar R. Bammi, for catching several bugs and providing the Unix makefiles
 * Byron T. Jenings Jr. for cleaning up the space code, providing a Unix
 *   manual page, and some ANSI and C++ improvements.
 * Skip Gilbrech for code to skip parameter names in prototypes.
 * ... and many others for helpful comments and suggestions.
 */
/* Modified 2014-09 to create fastspin flexc compatible __fromfile declarations 
 * To use it that way, compile this program and then do:
 *    libtool library/*.c > header.a
 * You may well have to fix up the output a bit, for example to replace type names
 * or to remove pointer to function declarations that were incorrectly flagged
 * as functions. But it'll get you 90% of the way there.
 */
/* if your compiler claims to be ANSI but doesn't have stddef.h or stdlib.h,
 * change the next line.
 * (and then complain to the supplier of the defective compiler)
 */

#if defined(__STDC__) && !defined(minix)
#include <stddef.h>
#include <stdlib.h>
#else
extern char *malloc();
extern long atol();
#endif

/* this stuff is non-standard, libtool has become Posix specific */
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS  0
#define EXIT_FAILURE  1
#endif

#include <stdio.h>
#include <ctype.h>
#include <string.h>

//#define DEBUG(s) (fputs(s, stderr)) /* */
#define DEBUG(s) /* */

#define ISCSYM(x) ((x) > 0 && (isalnum(x) || (x) == '_'))
#define ABORTED ( (Word *) -1 )
#define MAXPARAM 20 		/* max. number of parameters to a function */
#define NEWBUFSIZ (20480*sizeof(char)) /* new buffer size */

int dostatic = 0;		/* do static functions? */
int donum    = 0;		/* print line numbers? */
int define_macro   = 1;		/* define macro for prototypes? */
int use_macro   = 0;		/* use a macro for prototypes? */
char *macro_name = "P_";	/*   macro to use for prototypes */
int print_fromfile = 1;         /* print __fromfile() declaration for fastspin */
int no_parm_names = 0;		/* no parm names - only types */
int print_extern = 0;		/* use "extern" before function declarations */
#ifdef CPP
int call_cpp = 0;		/* preprocess files */
#endif

char *ourname;			/* our name, from argv[] array */
int inquote = 0;		/* in a quote?? */
int newline_seen = 1;		/* are we at the start of a line */
long linenum  = 1L;		/* line number in current file */
int glastc   = ' ';		/* last char. seen by getsym() */

int spin_output = 0;            /* to create a Spin library file rather than a .a one */

typedef struct word {
	struct word *next;
	char   string[1];
} Word;

#include "libtool.h"

/*
 * Routines for manipulating lists of words.
 */

Word *word_alloc(s)
	char *s;
{
	Word *w;

/* note that sizeof(Word) already contains space for a terminating null
 * however, we add 1 so that typefixhack can promote "float" to "double"
 *  by just doing a strcpy.
 */
	w = (Word *) malloc(sizeof(Word) + strlen(s) + 1);
	strcpy(w->string, s);
	w->next = NULL;
	return w;
}

void word_free(w)
	Word *w;
{
	Word *oldw;
	while (w) {
		oldw = w;
		w = w->next;
		free(oldw);
	}
}

/* return the length of a list; empty words are not counted */
int
List_len(w)
	Word *w;
{
	int count = 0;

	while (w) {
		if (*w->string) count++;
		w = w->next;
	}
	return count;
}

/* Append two lists, and return the result */

Word *word_append(w1, w2)
	Word *w1, *w2;
{
	Word *r, *w;

	r = w = word_alloc("");

	while (w1) {
		w->next = word_alloc(w1->string);
		w = w->next;
		w1 = w1->next;
	}
	while (w2) {
		w->next = word_alloc(w2->string);
		w = w->next;
		w2 = w2->next;
	}

	return r;
}
	
/* see if the last entry in w2 is in w1 */

int
foundin(w1, w2)
	Word *w1, *w2;
{
	while (w2->next)
		w2 = w2->next;

	while (w1) {
		if (!strcmp(w1->string, w2->string))
			return 1;
		w1 = w1->next;
	}
	return 0;
}

/* add the string s to the given list of words */

void addword(w, s)
	Word *w; char *s;
{
	while (w->next) w = w->next;
	w->next = word_alloc(s);
}

/* typefixhack: promote formal parameters of type "char", "unsigned char",
   "short", or "unsigned short" to "int".
*/

void typefixhack(w)
	Word *w;
{
	Word *oldw = 0;

	while (w) {
		if (*w->string) {
			if ( (!strcmp(w->string, "char") ||
			      !strcmp(w->string, "short") )
			    && (List_len(w->next) < 2) )
			{
/* delete any "unsigned" specifier present -- yes, it's supposed to do this */
				if (oldw && !strcmp(oldw->string, "unsigned")) {
					oldw->next = w->next;
					free(w);
					w = oldw;
				}
				strcpy(w->string, "int");
			}
			else if ( !strcmp(w->string, "float") &&
				  List_len(w->next) < 2 )
			{
				strcpy(w->string, "double");
			}
		}
		w = w->next;
	}
}

/* read a character: if it's a newline, increment the line count */

#ifdef __GNUC__	/* ++jrb */
inline
#endif
int ngetc(f)
	FILE *f;
{
	int c;

	c = getc(f);
	if (c == '\n') linenum++;

	return c;
}

/* read the next character from the file. If the character is '\' then
 * read and skip the next character. Any comment sequence is converted
 * to a blank.
 */

int fnextch(f)
	FILE *f;
{
	int c, lastc, incomment;

	c = ngetc(f);
	while (c == '\\') {
DEBUG("fnextch: in backslash loop\n");
		c = ngetc(f);	/* skip a character */
		c = ngetc(f);
	}
	if (c == '/' && !inquote) {
		c = ngetc(f);
		if (c == '*') {
			incomment = 1;
			c = ' ';
DEBUG("fnextch: comment seen\n");
			while (incomment) {
				lastc = c;
				c = ngetc(f);
				if (lastc == '*' && c == '/')
					incomment = 0;
				else if (c < 0)
					return c;
			}
			return fnextch(f);
		}
		else {
/* if we pre-fetched a linefeed, remember to adjust the line number */
			if (c == '\n') linenum--;
			ungetc(c, f);
			return '/';
		}
	}
	return c;
}


/* Get the next "interesting" character. Comments are skipped, and strings
 * are converted to "0". Also, if a line starts with "#" it is skipped.
 */

int nextch(f)
	FILE *f;
{
	int c, n;
	char *p, numbuf[10];

	c = fnextch(f);

/* skip preprocessor directives */
/* EXCEPTION: #line nnn or #nnn lines are interpreted */

	if (newline_seen && c == '#') {
/* skip blanks */
		do {
			c = fnextch(f);
		} while ( c >= 0 && (c == '\t' || c == ' ') );
/* check for #line */
		if (c == 'l') {
			c = fnextch(f);
			if (c != 'i')	/* not a #line directive */
				goto skip_rest_of_line;
			do {
				c = fnextch(f);
			} while (c >= 0 && c != '\n' && !isdigit(c));
		}

/* if we have a digit it's a line number, from the preprocessor */
		if (c >= 0 && isdigit(c)) {
			p = numbuf;
			for (n = 8; n >= 0; --n) {
				*p++ = c;
				c = fnextch(f);
				if (c <= 0 || !isdigit(c))
					break;
			}
			*p = 0;
			linenum = atol(numbuf) - 1;
		}

/* skip the rest of the line */
skip_rest_of_line:
		while (c >= 0 && c != '\n')
			c = fnextch(f);
		if (c < 0)
			return c;
	}
	newline_seen = (c == '\n');

	if (c == '\'' || c == '\"') {
DEBUG("nextch: in a quote\n");
		inquote = c;
		while ( (c = fnextch(f)) >= 0 ) {
			if (c == inquote) {
				inquote = 0;
DEBUG("nextch: out of quote\n");
				return '0';
			}
		}
DEBUG("nextch: EOF in a quote\n");
	}
	return c;
}

/*
 * Get the next symbol from the file, skipping blanks.
 * Return 0 if OK, -1 for EOF.
 * Also collapses everything between { and }
 */

int
getsym(buf, f)
	char *buf; FILE *f;
{
	register int c;
	int inbrack = 0;

DEBUG("in getsym\n");
	c = glastc;
	while ((c > 0) && isspace(c)) {
		c = nextch(f);
	}
DEBUG("getsym: spaces skipped\n");
	if (c < 0) {
DEBUG("EOF read in getsym\n");
		return -1;
	}
	if (c == '{') {
		inbrack = 1;
DEBUG("getsym: in bracket\n");
		while (inbrack) {
			c = nextch(f);
			if (c < 0) {
DEBUG("getsym: EOF seen in bracket loop\n");
				glastc = c;
				return c;
			}
			if (c == '{') inbrack++;
			else if (c == '}') inbrack--;
		}
		strcpy(buf, "{}");
		glastc = nextch(f);
DEBUG("getsym: out of in bracket loop\n");
		return 0;
	}
	if (!ISCSYM(c)) {
		*buf++ = c;
		*buf = 0;
		glastc = nextch(f);
DEBUG("getsym: returning special symbol\n");
		return 0;
	}
	while (ISCSYM(c)) {
		*buf++ = c;
		c = nextch(f);
	}
	*buf = 0;
	glastc = c;
DEBUG("getsym: returning word\n");
	return 0;
}

/*
 * skipit: skip until a ";" or the end of a function declaration is seen
 */
int skipit(buf, f)
	char *buf;
	FILE *f;
{
	int i;

	do {
DEBUG("in skipit loop\n");
		i = getsym(buf, f);
		if (i < 0) return i;
	} while (*buf != ';' && *buf != '{');

	return 0;
}

/*
 * find most common type specifiers for purpose of ruling them out as
 * parm names
 */

int is_type_word(s)
char *s;
{
    static char *typewords[] = {
	"char",		"const",	"double",	"enum",
	"float",	"int",		"long",		"short",
	"signed",	"struct",	"union",	"unsigned",
	"void",		"volatile",	(char *)0
    };

    register char **ss;

    for (ss = typewords; *ss; ++ss)
	if (strcmp(s, *ss) == 0)
	    return 1;

    return 0;
}


/* Ad-hoc macro to recognize parameter name for purposes of removal.
 * Idea is to remove the bulk of easily recognized parm names without
 * losing too many type specifiers. (sg)
 */
#define IS_PARM_NAME(w) \
	(ISCSYM(*(w)->string) && !is_type_word((w)->string) && \
	(!(w)->next || *(w)->next->string == ',' || \
	 *(w)->next->string == '['))


/*
 * given a list representing a type and a variable name, extract just
 * the base type, e.g. "struct word *x" would yield "struct word"
 */

Word *typelist(p)
	Word *p;
{
	Word *w, *r;

	r = w = word_alloc("");
	while (p && p->next) {
/* handle int *x --> int */
		if (p->string[0] && !ISCSYM(p->string[0]))
			break;
/* handle int x[] --> int */
		if (p->next->string[0] == '[')
			break;
		w->next = word_alloc(p->string);
		w = w->next;
		p = p->next;
	}
	return r;
}

/*
 * Get a parameter list; when this is called the next symbol in line
 * should be the first thing in the list.
 */

Word *getparamlist(f)
	FILE *f;
{
	static Word *pname[MAXPARAM]; /* parameter names */
	Word	*tlist,		  /* type name */
		*plist;		  /* temporary */
	int  	np = 0;		  /* number of parameters */
	int  	typed[MAXPARAM];  /* parameter has been given a type */
	int	tlistdone;	  /* finished finding the type name */
	int	sawsomething;
	int  	i;
	int	inparen = 0;
	char buf[80];

DEBUG("in getparamlist\n");
	for (i = 0; i < MAXPARAM; i++)
		typed[i] = 0;

	plist = word_alloc("");

/* first, get the stuff inside brackets (if anything) */

	sawsomething = 0;	/* gets set nonzero when we see an arg */
	for (;;) {
		if (getsym(buf, f) < 0) return NULL;
		if (*buf == ')' && (--inparen < 0)) {
			if (sawsomething) {	/* if we've seen an arg */
				pname[np] = plist;
				plist = word_alloc("");
				np++;
			}
			break;
		}
		if (*buf == ';') {	/* something weird */
			return ABORTED;
		}
		sawsomething = 1;	/* there's something in the arg. list */
		if (*buf == ',' && inparen == 0) {
			pname[np] = plist;
			plist = word_alloc("");
			np++;
		}
		else {
			addword(plist, buf);
			if (*buf == '(') inparen++;
		}
	}

/* next, get the declarations after the function header */

	inparen = 0;

	tlist = word_alloc("");
	plist = word_alloc("");
	tlistdone = 0;
	sawsomething = 0;
	for(;;) {
		if (getsym(buf, f) < 0) return NULL;

/* handle a list like "int x,y,z" */
		if (*buf == ',' && !inparen) {
			if (!sawsomething)
				return NULL;
			for (i = 0; i < np; i++) {
				if (!typed[i] && foundin(plist, pname[i])) {
					typed[i] = 1;
					word_free(pname[i]);
					pname[i] = word_append(tlist, plist);
				/* promote types */
					typefixhack(pname[i]);
					break;
				}
			}
			if (!tlistdone) {
				tlist = typelist(plist);
				tlistdone = 1;
			}
			word_free(plist);
			plist = word_alloc("");
		}
/* handle the end of a list */
		else if (*buf == ';') {
			if (!sawsomething)
				return ABORTED;
			for (i = 0; i < np; i++) {
				if (!typed[i] && foundin(plist, pname[i])) {
					typed[i] = 1;
					word_free(pname[i]);
					pname[i] = word_append(tlist, plist);
					typefixhack(pname[i]);
					break;
				}
			}
			tlistdone = 0;
			word_free(tlist); word_free(plist);
			tlist = word_alloc("");
			plist = word_alloc("");
		}
/* handle the  beginning of the function */
		else if (!strcmp(buf, "{}")) break;
/* otherwise, throw the word into the list (except for "register") */
		else if (strcmp(buf, "register")) {
			sawsomething = 1;
			addword(plist, buf);
			if (*buf == '(') inparen++;
			if (*buf == ')') inparen--;
		}
	}

/* Now take the info we have and build a prototype list */

/* empty parameter list means "void" */
	if (np == 0)
		return word_alloc("void");

	plist = tlist = word_alloc("");
	for (i = 0; i < np; i++) {

/* If no type provided, make it an "int" */
		if ( !(pname[i]->next) ||
	   (!(pname[i]->next->next)&&strcmp(pname[i]->next->string, "void"))) {
			addword(tlist, "int");
		}
		while (tlist->next) tlist = tlist->next;
		tlist->next = pname[i];
		if (i < np - 1)
			addword(tlist, ",");
	}
	return plist;
}

/*
 * emit a function declaration. The attributes and name of the function
 * are in wlist; the parameters are in plist. fname is the file name,
 * and startline is the starting line
 */

void emit(wlist, plist, fname, startline)
	Word *wlist, *plist;
        char *fname;
	long  startline;
{
	Word *w;
	int count = 0;
	int needspace = 0;
	int isstatic = 0;

DEBUG("emit called\n");
	if (donum)
		printf("/*%8ld */ ", startline);

	for (w = wlist; w; w = w->next) {
		if (w->string[0]) {
			count ++;
			if (!strcmp(w->string, "static"))
				isstatic = 1;
		}
	}

/* if the -e flag was given, and it's not a static function, print "extern" */

	if (print_extern && !isstatic) {
		printf("extern ");
	}

	if (count < 2) {
		printf("int");
		needspace = 1;
	}

	for (w = wlist; w; w = w->next) {
		if (needspace)
			putchar(' ');
		printf("%s", w->string);
		needspace = ISCSYM(w->string[0]);
	}
	if (use_macro)
		printf(" %s((", macro_name);
	else
		putchar('(');
	needspace = 0;
	for (w = plist; w; w = w->next) {
		if (no_parm_names && IS_PARM_NAME(w))
			continue;
		if (w->string[0] == ',')
			needspace = 1;
		else if (w->string[0] == '[')
			needspace = 0;
		else
		{
			if (needspace)
				putchar(' ');
			needspace = ISCSYM(w->string[0]);
		}
		printf("%s", w->string);
	}
	if (use_macro)
		printf("))");
	else
		printf(")");

        if (print_fromfile) {
            printf(" __fromfile(\"%s\")", fname);
        }
        printf(";\n");
}

/*
 * emit a function declaration for the Spin language. The attributes and name of the function
 * are in wlist; the parameters are in plist. fname is the file name,
 * and startline is the starting line
 */

void emit_spin(wlist, plist, fname, startline)
	Word *wlist, *plist;
        char *fname;
	long  startline;
{
	Word *w;
	int count = 0;
	int needspace = 0;
	int isstatic = 0;

DEBUG("emit called\n");
	if (donum)
		printf("{ %8ld } ", startline);

	for (w = wlist; w; w = w->next) {
		if (w->string[0]) {
			count ++;
			if (!strcmp(w->string, "static"))
				isstatic = 1;
		}
	}

/* if the -e flag was given, and it's not a static function, print "extern" */

        if (isstatic) {
            printf("PRI ");
        } else {
            printf("PUB ");
        }
        printf("FILE \"%s\" ", fname);
        
	for (w = wlist; w; w = w->next) {
		if (needspace)
			putchar(' ');
                if (!w->next) {
                    printf("%s", w->string);
                }
	}
        putchar('(');
	needspace = 0;
	for (w = plist; w; w = w->next) {
//		if (no_parm_names && IS_PARM_NAME(w))
//			continue;
		if (w->string[0] == ',') {
			needspace = 1;
        	}
                else
		{
                    if (!IS_PARM_NAME(w)) {
                        continue;
                    }
                    if (needspace) {
                        putchar(' ');
                        needspace = 0;
                    }
		}
		printf("%s", w->string);
	}
        printf(")");

        printf("\n");
}

/*
 * get all the function declarations
 */

void getdecl(f, fname)
	FILE *f;
        char *fname;
{
	Word *plist, *wlist = NULL;
	char buf[80];
	int sawsomething;
	long startline;		/* line where declaration started */
	int oktoprint;
again:
	word_free(wlist);
	wlist = word_alloc("");
	sawsomething = 0;
	oktoprint = 1;

	for(;;) {
DEBUG("main getdecl loop\n");
		if (getsym(buf,f) < 0) {
DEBUG("EOF in getdecl loop\n");
			 return;
		}
/* try to guess when a declaration is not an external function definition */
		if (!strcmp(buf, ",") || !strcmp(buf, "{}") ||
		    !strcmp(buf, "=") || !strcmp(buf, "typedef") ||
		    !strcmp(buf, "extern")) {
			skipit(buf, f);
			goto again;
		}
		if (!dostatic && !strcmp(buf, "static")) {
			oktoprint = 0;
		}
/* for the benefit of compilers that allow "inline" declarations */
		if (!strcmp(buf, "inline") && !sawsomething)
			continue;
		if (!strcmp(buf, ";")) goto again;

/* A left parenthesis *might* indicate a function definition */
		if (!strcmp(buf, "(")) {
			startline = linenum;
			if (!sawsomething || !(plist = getparamlist(f))) {
				skipit(buf, f);
				goto again;
			}
			if (plist == ABORTED)
				goto again;

/* It seems to have been what we wanted */
			if (oktoprint) {
                            if (spin_output) {
                                emit_spin(wlist, plist, fname, startline);
                            } else {
                                emit(wlist, plist, fname, startline);
                            }
                        }
			word_free(plist);
			goto again;
		}
		addword(wlist, buf);
		sawsomething = 1;
	}
}

// forward declaration
static int scan_directory(char *filename);

void
dofile(char *filename)
{
    FILE *f;
    char *iobuf;

    if (scan_directory(filename)) {
        return;
    }
    iobuf = malloc(NEWBUFSIZ);
    
    if (!(f = fopen(filename, "r"))) {
        perror(filename);
        exit(EXIT_FAILURE);
    }
    if (iobuf)
        setvbuf(f, iobuf, _IOFBF, NEWBUFSIZ);

    if (spin_output) {
        printf("\n{ %s }\n", filename);
    } else {
        printf("\n/* %s */\n", filename);
    }
    linenum = 1;
    newline_seen = 1;
    glastc = ' ';
    DEBUG("calling getdecl\n");
    getdecl(f, filename);
    DEBUG("back from getdecl\n");
    fclose(f);
    DEBUG("back from fclose\n");
    free(iobuf);
}

// check for whether a file contains C code
static int is_c_file(const char *filename)
{
    char *ext = strrchr(filename, '.');
    if (!ext) return 0;
    if (!strcmp(ext, ".c")) return 1;
    if (!strcmp(ext, ".C")) return 1;
    if (!strcmp(ext, ".cpp")) return 1;
    if (!strcmp(ext, ".cc")) return 1;
    if (!strcmp(ext, ".cxx")) return 1;
    if (!strcmp(ext, ".c++")) return 1;
    return 0;
}

// try to recursively scan a directory
static int
scan_directory(char *filename)
{
    DIR *dir;
    struct dirent *ent;
    
    dir = opendir(filename);
    if (!dir) return 0;
    if (chdir(filename) < 0) {
        closedir(dir);
        return 0;
    }
    for(;;) {
        ent = readdir(dir);
        if (!ent) break;
        if (scan_directory(ent->d_name)) {
            continue;
        }
        if (is_c_file(ent->d_name)) {
            dofile(ent->d_name);
        }
    }
    chdir("..");
    closedir(dir);
    return 1;
}

int
main(argc, argv)
int argc; char **argv;
{
	extern void Usage();
        char *t;
        
	if (argv[0] && argv[0][0])
		ourname = argv[0];
	else
		ourname = "libtool";

	argv++; argc--;

	if (argc < 0)		/* strange -- no args at all */
		Usage();

	while (*argv && **argv == '-') {
		t = *argv++; --argc; t++;
		while (*t) {
			if (*t == 'e')
				print_extern = 1;
			else if (*t == 'n')
				donum = 1;
			else if (*t == 'p') {
				t = *argv++; --argc;
				if (!t)
					Usage();
				use_macro = 1;
				macro_name = t;
				break;
			}
			else if (*t == 's')
				dostatic = 1;
			else if (*t == 'x')
				/* no parm names, only types (sg) */
				no_parm_names = 1;
			else if (*t == 'z')
				define_macro = 0;
			else if (*t == 'A')
				use_macro = 0;
			else if (*t == 'S')
                        	spin_output = 1;
			else
				Usage();
			t++;
		}
	}

	if (use_macro && define_macro) {
		printf("#if defined(__STDC__) || defined(__cplusplus)\n");
		printf("# define %s(s) s\n", macro_name);
		printf("#else\n");
		printf("# define %s(s) ()\n", macro_name);
		printf("#endif\n\n");
	}
	if (argc == 0)
                getdecl(stdin, "<stdin>");
	else
		while (argc > 0 && *argv) {
                    dofile(*argv);
                    argc--; argv++;
		}
	if (use_macro && define_macro) {
		printf("\n#undef %s\n", macro_name);	/* clean up namespace */
	}
	return(EXIT_SUCCESS);
}


void Usage()
{
	fprintf(stderr, 
	   "Usage: %s [-e][-n][-p sym][-s][-x][-z][-A][files ...]\n", ourname);
	fputs("   -e: put an explicit \"extern\" keyword in declarations\n",
	   stderr);
	fputs("   -n: put line numbers of declarations as comments\n",stderr);
	fputs("   -p nm: use \"nm\" as the prototype macro (default \"_P\")\n",
	   stderr);
	fputs("   -s: include declarations for static functions\n", stderr);
	fputs("   -x: omit parameter names in prototypes\n", stderr);
	fputs("   -z: omit prototype macro definition\n", stderr);
	fputs("   -A: omit prototype macro; header files are strict ANSI\n",
	   stderr);
	exit(EXIT_FAILURE);
}
