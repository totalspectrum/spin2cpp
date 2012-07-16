#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include "spinc.h"

//#define DEBUG_YACC

extern int yyparse(void);

extern int yydebug;

ParserState *current;
ParserState *allparse;

int gl_errors;
AST *ast_type_word, *ast_type_long, *ast_type_byte;

int
yylex(YYSTYPE *lvalp)
{
    int c;
    c = getToken(&current->L, &current->ast);
    if (c == T_EOF)
        return 0;
    return c;
}

const char *gl_progname = "spin2cpp";

/*
 * make sure that a class name is safe, i.e. will
 * not conflict with any identifier or C keyword/function
 * Identifiers will have exactly 1 capital letter (the first),
 * so if there is any capital letter other than the first then
 * we are fine. If the class name is all lower case, but has
 * some digits or underscores, then it's also OK. Otherwise
 * add a "Spin" to the end
 */
static void
makeClassNameSafe(char *classname)
{
    char *s;
    int hasUpper = 0;
    int hasDigit = 0;
    int ok = 0;

    for (s = classname; *s; s++) {
        if (isupper(*s))
            hasUpper++;
        else if (isdigit(*s) || *s == '_')
            hasDigit++;
        else if (!islower(*s)) {
            *s = '_';
            hasDigit++;
        }
    }
    ok = (hasUpper > 1) || (hasUpper == 1 && !isupper(classname[0]))
        || (hasUpper == 0 && hasDigit > 0);

    if (!ok) {
        strcat(classname, "Spin");
    }
}

/*
 * allocate a new parser state
 */ 
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
    s = strrchr(P->basename, '.');
    if (s) *s = 0;

    /* set up the class name */
    s = strrchr(P->basename, '/');
    /* allocate enough space to append "Spin" if necessary */
    P->classname = calloc(1, strlen(P->basename)+5);
    if (s)
        strcpy(P->classname, s+1);
    else
        strcpy(P->classname, P->basename);
#if defined(WIN32)
    s = strrchr(P->classname, '\\');
    if (s)
        P->classname = s+1;
#endif
    makeClassNameSafe(P->classname);
    return P;
}

/*
 * declare constant symbols
 */
static Symbol *
EnterConstant(const char *name, AST *expr)
{
    Symbol *sym;

    if (IsFloatConst(expr)) {
        sym = AddSymbol(&current->objsyms, name, SYM_FLOAT_CONSTANT, (void *)expr);
    } else {
        sym = AddSymbol(&current->objsyms, name, SYM_CONSTANT, (void *)expr);
    }
    return sym;
}

void
DeclareConstants(AST *conlist)
{
    AST *upper, *ast;
    int default_val = 0;

    for (upper = conlist; upper; upper = upper->right) {
        if (upper->kind == AST_LISTHOLDER) {
            ast = upper->left;
            while (ast) {
                switch (ast->kind) {
                case AST_ENUMSET:
                    default_val = EvalConstExpr(ast->left);
                    ast = ast->right;
                    break;
                case AST_IDENTIFIER:
                    EnterConstant(ast->d.string, AstInteger(default_val));
                    default_val++;
                    ast = ast->right;
                    break;
                case AST_ASSIGN:
                    EnterConstant(ast->left->d.string, ast->right);
                    ast = NULL;
                    break;
                default:
                    ERROR(ast, "Internal error: bad AST value %d", ast->kind);
                    ast = NULL;
                    break;
                }
            } 
        } else {
            ERROR(upper, "Expected list in constant, found %d instead", upper->kind);
        }
    }
}

static void
DeclareVariables(void)
{
    AST *upper;
    AST *curtype;

    for (upper = current->varblock; upper; upper = upper->right) {
        switch (upper->kind) {
        case AST_BYTELIST:
            curtype = ast_type_byte;
            break;
        case AST_WORDLIST:
            curtype = ast_type_word;
            break;
        case AST_LONGLIST:
            curtype = ast_type_long;
            break;
        default:
            ERROR(upper, "bad type  %d in variable list\n", upper->kind);
            return;
        }
        EnterVars(SYM_VARIABLE, &current->objsyms, curtype, upper->left);
    }
}

static void
DeclareObjects(ParserState *parse)
{
    AST *ast;

    for (ast = parse->objblock; ast; ast = ast->right) {
        if (ast->kind != AST_OBJECT) {
            ERROR(ast, "Internal error: expected an OBJECT");
            break;
        }
        AddSymbol(&current->objsyms, ast->left->d.string, SYM_OBJECT, ast->d.ptr);
    }
}

/*
 * parse a file
 * This is the main entry point for the compiler
 * "name" is the file name; if it has no .spin suffix
 * we'll try it with one
 * "printMain" is nonzero if we should emit a main() function
 * that calls the first method in the object
 */
ParserState *
parseFile(const char *name)
{
    FILE *f = NULL;
    ParserState *P, *save, *Q;
    char *fname = NULL;

    fname = malloc(strlen(name) + 8);
    f = fopen(name, "r");
    if (!f) {
        sprintf(fname, "%s.spin", name);
        f = fopen(fname, "r");
    }
    if (!f) {
        perror(name);
        free(fname);
        exit(1);
    }
    save = current;
    P = NewParserState(name);
    /* if we have already visited an object with this name, skip it */
    for (Q = allparse; Q; Q = Q->next) {
        if (!strcmp(P->basename, Q->basename)) {
            free(fname);
            free(P);
            fclose(f);
            return Q;
        }
    }

    P->next = allparse;
    allparse = P;
    current = P;

    fileToLex(&current->L, f, name);
    yyparse();
    fclose(f);

    if (gl_errors > 0) {
        fprintf(stderr, "%d errors\n", gl_errors);
        free(fname);
        exit(1);
    }

    /* now declare all the symbols that weren't already declared */
    DeclareObjects(P);
    DeclareVariables();
    DeclareLabels(P);

    if (gl_errors > 0) {
        fprintf(stderr, "%d errors\n", gl_errors);
        free(fname);
        exit(1);
    }


    current = save;
    return P;
}

AST *
NewObject(AST *identifier, AST *string)
{
    AST *ast;

    ast = NewAST(AST_OBJECT, identifier, NULL);
    ast->d.ptr = parseFile(string->d.string);
    return ast;
}

void
ERROR(AST *instr, const char *msg, ...)
{
    va_list args;

    if (instr)
        fprintf(stderr, "Error: %s line %d: ", instr->fileName, instr->line);
    else
        fprintf(stderr, "Error: ");

    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    fprintf(stderr, "\n");
    gl_errors++;
}

static void
init()
{
    ast_type_long = NewAST(AST_INTTYPE, AstInteger(4), NULL);
    ast_type_word = NewAST(AST_UNSIGNEDTYPE, AstInteger(2), NULL);
    ast_type_byte = NewAST(AST_UNSIGNEDTYPE, AstInteger(1), NULL);

    initLexer();
}

static void
Usage(void)
{
    fprintf(stderr, "Spin to C++ converter version %s\n", VERSIONSTR);
    fprintf(stderr, "Usage: %s [--main][--dat] file.spin\n", gl_progname);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --main:    include C++ main() function\n");
    fprintf(stderr, "  --dat:     output binary blob of DAT section only\n");
    fprintf(stderr, "  --files:   print list of .cpp files to stdout\n");
    exit(2);
}

int
main(int argc, char **argv)
{
    int outputMain = 0;
    int outputDat = 0;
    int outputFiles = 0;
    ParserState *P;

    init();

    allparse = NULL;
#ifdef DEBUG_YACC
    yydebug = 1;  /* turn on yacc debugging */
#endif
    /* parse arguments */
    if (argv[0] != NULL) {
        gl_progname = argv[0];
        argv++; --argc;
    }
    while (argv[0] && argv[0][0] == '-') {
        if (!strcmp(argv[0], "-y")) {
            yydebug = 1;
            argv++; --argc;
        } else if (!strncmp(argv[0], "--m", 3) || !strncmp(argv[0], "-m", 2)) {
            outputMain = 1;
            argv++; --argc;
        } else if (!strncmp(argv[0], "--dat", 5)) {
            outputDat = 1;
            argv++; --argc;
        } else if (!strncmp(argv[0], "--file", 7)) {
            outputFiles = 1;
            argv++; --argc;
        } else {
            Usage();
        }
    }
    if (argv[0] == NULL) {
        Usage();
    }
    P = parseFile(argv[0]);
    if (P) {
        if (outputDat) {
            OutputDatFile(P->basename, P);
        } else {
            ParserState *Q;

            /* compile any sub-objects needed */
            for (Q = allparse; Q; Q = Q->next) {
                OutputCppCode(Q->basename, Q, outputMain && Q == P);
                if (outputFiles) {
                    printf("%s.cpp\n", Q->basename);
                }
            }
        }
    }
    return 0;
}
