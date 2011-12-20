#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "spinc.h"

#define DEBUG_YACC

extern int yyparse(void);

extern int yydebug;

ParserState *current;
int gl_errors;

int
yylex(void)
{
    int c;
    c = getToken(&current->L, &current->ast);
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

ParserState *
NewParserState(const char *name)
{
    ParserState *P;
    char *s;

    P = calloc(1, sizeof(*P));
    if (!P) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    /* set up the base file name */
    P->basename = strdup(name);
    s = rindex(P->basename, '.');
    if (s) *s = 0;

    /* set up the class name */
    s = rindex(P->basename, '/');
    if (s)
        P->classname = strdup(s+1);
    else
        P->classname = strdup(P->basename);
#if defined(WIN32)
    s = rindex(P->classname, '\\');
    if (s)
        P->classname = s+1;
#endif

    return P;
}

/*
 * declare constant symbols
 */
static Symbol *
EnterConstant(const char *name, long val)
{
    Symbol *sym;

    sym = AddSymbol(&current->objsyms, name, SYM_CONSTANT, (void *)val);
    return sym;
}

static void
DeclareConstants(void)
{
    AST *ast, *sub;
    int default_val = 0;

    for (ast = current->conblock; ast; ast = ast->right) {
        switch (ast->kind) {
        case AST_ENUMSET:
            default_val = EvalConstExpr(ast->left);
            break;
        case AST_IDENTIFIER:
            EnterConstant(ast->d.string, default_val);
            default_val++;
            break;
        case AST_CONDECL:
            sub = ast->left;
            if (sub->kind == AST_ASSIGN) {
                EnterConstant(sub->left->d.string, EvalConstExpr(sub->right));
            } else {
                ERROR("Internal error: expected constant assignment");
            }
            break;
        default:
            ERROR("Internal error: bad AST value");
            break;
        } 
    }
}

/*
 * print out a header file
 */
static void
PrintConstantDecl(FILE *f, AST *ast)
{
    fprintf(f, "  const int %s = %ld; \n", ast->d.string, EvalConstExpr(ast));
}

static void
PrintHeaderFile(FILE *f)
{
    AST *ast;
    fprintf(f, "class %s {\n", current->classname);
    for (ast = current->conblock; ast; ast = ast->right) {
        switch (ast->kind) {
        case AST_IDENTIFIER:
            PrintConstantDecl(f, ast);
            break;
        case AST_CONDECL:
            PrintConstantDecl(f, ast->left->left);
            break;
        default:
            /* do nothing */
            break;
        }
    }
    fprintf(f, "};\n");
}

/*
 * parse a file
 */
static void
parseFile(const char *name)
{
    FILE *f;
    ParserState *P;
    char *fname;

    f = fopen(name, "r");
    if (!f) {
        perror(name);
        exit(1);
    }
    P = NewParserState(name);
    P->next = current;
    current = P;

    fileToLex(&current->L, f, name);
    yyparse();
    fclose(f);

    /* now declare all the symbols */
    DeclareConstants();

    /* print out the header file */
    fname = malloc(strlen(current->basename + 8));
    sprintf(fname, "%s.h", current->basename);
    f = fopen(fname, "w");
    if (!f) {
        perror(fname);
        exit(1);
    }
    PrintHeaderFile(f);
    fclose(f);
}

void
ERROR(const char *msg, ...)
{
    va_list args;

    fprintf(stderr, "Error: ");
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);

    gl_errors++;
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
