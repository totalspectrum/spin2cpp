#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
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

const char *gl_progname = "spintoc";

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
    if (s)
        P->classname = strdup(s+1);
    else
        P->classname = strdup(P->basename);
#if defined(WIN32)
    s = strrchr(P->classname, '\\');
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
                ERROR(ast, "Internal error: bad AST value %d", ast->kind);
                break;
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
        EnterVars(&current->objsyms, curtype, upper->left);
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
 * print out a header file
 */
static void
PrintConstantDecl(FILE *f, AST *ast)
{
    fprintf(f, "  static const int %s = %d;\n", ast->d.string, (int)EvalConstExpr(ast));
}


static void
PrintHeaderFile(FILE *f, ParserState *parse)
{
    AST *ast, *upper, *sub;
    int already_done;
    ParserState *objstate;

    /* things we always need */
    fprintf(f, "#ifndef %s_class_defined__\n", parse->classname);
    fprintf(f, "#define %s_class_defined__\n\n", parse->classname);

    fprintf(f, "#include <stdint.h>\n");

    /* include any needed object headers */
    for (ast = parse->objblock; ast; ast = ast->right) {
        if (ast->kind != AST_OBJECT) {
            ERROR(ast, "Internal error: expected an OBJECT");
            break;
        }

        /* see if we've already printed this header */
        objstate = ast->d.ptr;
        already_done = 0;
        for (sub = parse->objblock; sub && sub != ast; sub = sub->right) {
            if (sub->d.ptr == objstate) {
                already_done = 1;
                break;
            }
        }
        if (!already_done) {
            fprintf(f, "#include \"%s.h\"\n", objstate->basename);
        }
    }
    fprintf(f, "\n");

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
    /* object references */
    for (ast = parse->objblock; ast; ast = ast->right) {
        ParserState *P = ast->d.ptr;
        fprintf(f, "  %s\t%s;\n", P->classname, ast->left->d.string);
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
            PrintVarList(f, ast_type_byte, ast->left);
            break;
        case AST_WORDLIST:
            PrintVarList(f, ast_type_word, ast->left);
            break;
        case AST_LONGLIST:
            PrintVarList(f, ast_type_long, ast->left);
            break;
        default:
            break;
        }
    }
    /* now the private methods */
    PrintPrivateFunctionDecls(f, parse);
    fprintf(f, "};\n\n");
    fprintf(f, "#endif\n");
}

static void
PrintMacros(FILE *f, ParserState *parse)
{
    AST *ast;
    if (parse->needsYield) {
        fprintf(f, "#define Yield__() (__napuntil(_CNT + 256))\n");
    }
    if (parse->needsMinMax) {
        fprintf(f, "#define Min__(x, y) __extension__({ int32_t a = (x); int32_t b = (y); a < b ? a : b;})\n"); 
        fprintf(f, "#define Max__(x, y) __extension__({ int32_t a = (x); int32_t b = (y); a > b ? a : b;})\n\n");
    }
    if (parse->needsRotate) {
        fprintf(f, "#define Rotl__(x, y) __extension__({ uint32_t a = (x); uint32_t b = (y); (a<<b) | (a>>(32-b)); })\n"); 
        fprintf(f, "#define Rotr__(x, y) __extension__({ uint32_t a = (x); uint32_t b = (y); (a>>b) | (a<<(32-b)); })\n\n"); 
    }

    if (parse->arrays) {
        fprintf(f, "#define Lookup__(x, a) __extension__({ int32_t i = (x); (i < 0 || i >= sizeof(a)/sizeof(a[0])) ? 0 : a[i]; })\n");
        for (ast = parse->arrays; ast; ast = ast->right) {
            PrintLookupArray(f, ast->left);
        }
        fprintf(f, "\n");
    }
}

static void
PrintCppFile(FILE *f, ParserState *parse)
{
    /* things we always need */
    if (parse->needsStdlib) {
        fprintf(f, "#include <stdlib.h>\n");
    }
    if (parse->needsYield) {
        fprintf(f, "#include <sys/thread.h>\n");
    }
    fprintf(f, "#include <propeller.h>\n");
    fprintf(f, "#include \"%s.h\"\n", parse->basename);
    fprintf(f, "\n");
    PrintMacros(f, parse);

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
static ParserState *
parseFile(const char *name, int printMain)
{
    FILE *f;
    ParserState *P, *save;
    char *fname;

    f = fopen(name, "r");
    if (!f) {
        perror(name);
        exit(1);
    }
    save = current;
    P = NewParserState(name);
    P->next = allparse;
    allparse = P;
    current = P;

    fileToLex(&current->L, f, name);
    yyparse();
    fclose(f);

    if (gl_errors > 0) {
        fprintf(stderr, "%d errors\n", gl_errors);
        exit(1);
    }

    /* now declare all the symbols */
    DeclareObjects(P);
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
    if (printMain) {
        Function *defaultMethod = P->functions;
        if (defaultMethod == NULL) {
            ERROR(NULL, "unable to find default method for %s", P->classname);
            goto done;
        }
        if (defaultMethod->params) {
            ERROR(NULL, "default method of %s expects parameters", P->classname);
            goto done;
        }
        fprintf(f, "\n");
        fprintf(f, "%s MainObj__;\n\n", P->classname);
        fprintf(f, "int main() {\n");
        fprintf(f, "  return MainObj__.%s();\n", defaultMethod->name);
        fprintf(f, "}\n");
    }

done:
    fclose(f);

    if (gl_errors > 0) {
        fprintf(stderr, "%d errors\n", gl_errors);
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
    ast->d.ptr = parseFile(string->d.string, 0);
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
    fprintf(stderr, "Usage: %s [-main] file.spin\n", gl_progname);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -main:    include C++ main() function\n");
    exit(2);
}

int
main(int argc, char **argv)
{
    int outputMain = 0;
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
        } else if (!strncmp(argv[0], "-m", 2)) {
            outputMain = 1;
            argv++; --argc;
        } else {
            Usage();
        }
    }
    if (argv[0] == NULL) {
        Usage();
    }
    parseFile(argv[0], outputMain);
    return 0;
}
