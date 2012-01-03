#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "spinc.h"

//#define DEBUG_YACC

extern int yyparse(void);

extern int yydebug;

ParserState *current;
int gl_errors;
AST *ast_type_word, *ast_type_long, *ast_type_byte;

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
    AST *upper, *ast;
    int default_val = 0;

    for (upper = current->conblock; upper; upper = upper->right) {
        if (upper->kind == AST_LISTHOLDER) {
            ast = upper->left;
            switch (ast->kind) {
            case AST_ENUMSET:
                default_val = EvalConstExpr(ast->left);
                break;
            case AST_IDENTIFIER:
                EnterConstant(ast->d.string, default_val);
                default_val++;
                break;
            case AST_ASSIGN:
                EnterConstant(ast->left->d.string, EvalConstExpr(ast->right));
                break;
            default:
                ERROR("Internal error: bad AST value %d", ast->kind);
                break;
            } 
        } else {
            ERROR("Expected list in constant, found %d instead", upper->kind);
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
            ERROR("bad type  %d in variable list\n", upper->kind);
            return;
        }
        EnterVars(&current->objsyms, curtype, upper->left);
    }
}

/*
 * print out a header file
 */
static void
PrintConstantDecl(FILE *f, AST *ast)
{
    fprintf(f, "  static const int %s = %d;\n", ast->d.string, (int)EvalConstExpr(ast));
}

static void
PrintVarList(FILE *f, const char *typename, AST *ast)
{
    AST *decl;
    int needcomma = 0;

    fprintf(f, "  %s\t", typename);
    while (ast != NULL) {
        if (needcomma) {
            fprintf(f, ", ");
        }
        needcomma = 1;
        if (ast->kind != AST_LISTHOLDER) {
            ERROR("Expected variable list element\n");
            return;
        }
        decl = ast->left;
        switch (decl->kind) {
        case AST_IDENTIFIER:
            fprintf(f, "%s", decl->d.string);
            break;
        case AST_ARRAYDECL:
            fprintf(f, "%s[%d]", decl->left->d.string,
                    (int)EvalConstExpr(decl->right));
            break;
        default:
            ERROR("Internal problem in variable list: type=%d\n", decl->kind);
            break;
        }
        ast = ast->right;
    }
    fprintf(f, ";\n");
}

static void
PrintHeaderFile(FILE *f, ParserState *parse)
{
    AST *ast, *upper;

    /* things we always need */
    fprintf(f, "#include <stdint.h>\n\n");

    /* print the constant declarations */
    fprintf(f, "class %s {\npublic:\n", parse->classname);
    for (upper = parse->conblock; upper; upper = upper->right) {
        ast = upper->left;
        switch (ast->kind) {
        case AST_IDENTIFIER:
            PrintConstantDecl(f, ast);
            break;
        case AST_ASSIGN:
            PrintConstantDecl(f, ast->left);
            break;
        default:
            /* do nothing */
            break;
        }
    }
    /* data block, if applicable */
    if (parse->datblock) {
        fprintf(f, "  static uint8_t dat[];\n");
    }
    /* now the public members */
    PrintPublicFunctionDecls(f, parse);

    /* now the private members */
    fprintf(f, "private:\n");
    for (ast = parse->varblock; ast; ast = ast->right) {
        switch (ast->kind) {
        case AST_BYTELIST:
            PrintVarList(f, "uint8_t", ast->left);
            break;
        case AST_WORDLIST:
            PrintVarList(f, "uint16_t", ast->left);
            break;
        case AST_LONGLIST:
            PrintVarList(f, "int32_t", ast->left);
            break;
        default:
            break;
        }
    }
    /* now the private methods */
    PrintPrivateFunctionDecls(f, parse);
    fprintf(f, "};\n");
}

static void
PrintCppFile(FILE *f, ParserState *parse)
{
    /* things we always need */
    fprintf(f, "#include <propeller.h>\n");
    fprintf(f, "#include \"%s.h\"\n", parse->basename);
    fprintf(f, "\n");
    /* print data block, if applicable */
    if (parse->datblock) {
        fprintf(f, "uint8_t %s::dat[] = {\n", parse->classname);
        PrintDataBlock(f, parse);
        fprintf(f, "};\n");
    }
    /* functions */
    PrintFunctionBodies(f, parse);
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
    DeclareVariables();

    if (gl_errors > 0) {
        fprintf(stderr, "%d errors\n", gl_errors);
        exit(1);
    }

    /* print out the header file */
    fname = malloc(strlen(P->basename + 8));
    sprintf(fname, "%s.h", P->basename);
    f = fopen(fname, "w");
    if (!f) {
        perror(fname);
        exit(1);
    }
    PrintHeaderFile(f, P);
    fclose(f);

    if (gl_errors > 0) {
        fprintf(stderr, "%d errors\n", gl_errors);
        exit(1);
    }

    sprintf(fname, "%s.cpp", P->basename);
    f = fopen(fname, "w");
    if (!f) {
        perror(fname);
        exit(1);
    }
    PrintCppFile(f, P);
    fclose(f);

    if (gl_errors > 0) {
        fprintf(stderr, "%d errors\n", gl_errors);
        exit(1);
    }

}

void
ERROR(const char *msg, ...)
{
    va_list args;

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

int
main(int argc, char **argv)
{
    init();

#ifdef DEBUG_YACC
    yydebug = 1;  /* turn on yacc debugging */
#endif
    /* parse arguments */
    if (argv[0] != NULL) {
        gl_progname = argv[0];
        argv++; --argc;
    }
    if (argv[0] && !strcmp(argv[0], "-y")) {
        yydebug = 1;
        argv++; --argc;
    }
    if (argv[0] == NULL) {
        Usage();
    }
    parseFile(argv[0]);
    return 0;
}
