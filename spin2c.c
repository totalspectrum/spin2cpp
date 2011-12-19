#include "spinc.h"
#include "lexer.h"

extern int yyparse(void);

LexStream L;
AST *ast;

int
yylex(void)
{
    int c;
    c = getToken(&L, &ast);
    if (c == T_IDENTIFIER) {
        printf("read identifier [%s]\n", ast->d.string);
    } else {
        printf("read token %d\n", c);
    }
    if (c == T_EOF)
        return 0;
    return c;
}

int
main(int argc, char **argv)
{
    extern int yydebug;

    initLexer();

    yydebug = 1;  /* turn on yacc debugging */
    fileToLex(&L, stdin, "<stdin>");
    yyparse();
    return 0;
}
