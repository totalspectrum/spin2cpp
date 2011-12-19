#include "spinc.h"
#include "lexer.h"
#include <stdlib.h>

extern int yyparse(void);

extern int yydebug;

LexStream L;
AST *ast;

int
yylex(void)
{
    int c;
    c = getToken(&L, &ast);
#if 0
    if (c == T_IDENTIFIER) {
        printf("read identifier [%s]\n", ast->d.string);
    } else {
        printf("read token %d\n", c);
    }
#endif
    if (c == T_EOF)
        return 0;
    return c;
}

const char *gl_progname = "spintoc";

static void
Usage(void)
{
    fprintf(stderr, "Usage: %s file.spin\n", gl_progname);
    exit(2);
}

static void
parseFile(const char *name)
{
    FILE *f;

    f = fopen(name, "r");
    if (!f) {
        perror(name);
        exit(1);
    }
    fileToLex(&L, f, name);
    yyparse();
}

int
main(int argc, char **argv)
{
    initLexer();

#ifdef DEBUG_YACC
    yydebug = 1;  /* turn on yacc debugging */
#endif
    /* parse arguments */
    if (argv[0] != NULL) {
        gl_progname = argv[0];
        argv++; --argc;
    }
    if (argv[0] == NULL) {
        Usage();
    }
    parseFile(argv[0]);
    return 0;
}
