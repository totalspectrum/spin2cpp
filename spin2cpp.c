#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>
#include "spinc.h"
#include "preprocess.h"

//#define DEBUG_YACC

extern int yyparse(void);

extern int yydebug;

ParserState *current;
ParserState *allparse;

int gl_errors;
int gl_ccode;
int gl_nospin;
int gl_static;
AST *ast_type_word, *ast_type_long, *ast_type_byte;

struct preprocess gl_pp;

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
char *gl_header = NULL;

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
DeclareVariables(ParserState *P)
{
    AST *upper;
    AST *curtype;

    for (upper = P->varblock; upper; upper = upper->right) {
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

void
DeclareObjects(AST *newobjs)
{
    AST *ast;
    AST *obj;

    for (ast = newobjs; ast; ast = ast->right) {
        if (ast->kind != AST_OBJECT) {
            ERROR(ast, "Internal error: expected an OBJECT");
            break;
        }
        obj = ast->left;
        if (obj->kind == AST_IDENTIFIER) {
            AddSymbol(&current->objsyms, ast->left->d.string, SYM_OBJECT, ast);
        } else if (obj->kind == AST_ARRAYDECL) {
            AST *id = obj->left;
            AddSymbol(&current->objsyms, id->d.string, SYM_OBJECT, ast);
        } else {
            ERROR(ast, "Internal error: bad object definition");
        }
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
    ParserState *P, *save, *Q, *LastQ;
    char *fname = NULL;
    char *parseString = NULL;

    fname = malloc(strlen(name) + 8);
    strcpy(fname, name);
    f = fopen(fname, "r");
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
    /* also finds the last element in the list, so we can append to
       the list easily */
    LastQ = NULL;
    for (Q = allparse; Q; Q = Q->next) {
        if (!strcmp(P->basename, Q->basename)) {
            free(fname);
            free(P);
            fclose(f);
            if (gl_static) {
                /* this is bad, we can't have two copies of an object */
                ERROR(NULL, "multiple use of object %s not allowed in C mode",
                      P->basename);
            }
            return Q;
        }
        LastQ = Q;
    }
    if (LastQ)
        LastQ->next = P;
    else
        allparse = P;
    current = P;

    if (gl_preprocess) {
        void *defineState;

        pp_push_file(&gl_pp, fname);
        defineState = pp_get_define_state(&gl_pp);
        pp_run(&gl_pp);
        parseString = pp_finish(&gl_pp);
        pp_restore_define_state(&gl_pp, defineState);

        strToLex(&current->L, parseString, fname);
        yyparse();
        free(parseString);
    } else {
        fileToLex(&current->L, f, fname);
        yyparse();
    }
    fclose(f);

    if (gl_errors > 0) {
        fprintf(stderr, "%d errors\n", gl_errors);
        free(fname);
        exit(1);
    }

    /* now declare all the symbols that weren't already declared */
    DeclareConstants(P->conblock);
    DeclareVariables(P);
    DeclareLabels(P);
    DeclareFunctions(P);

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
    const char *filename = string->d.string;

    if (gl_static && identifier->kind == AST_ARRAYDECL) {
        /* this is bad, we can't have two copies of an object */
        ERROR(NULL, "multiple use of object %s not allowed in C mode",
              filename);
    }
    ast = NewAST(AST_OBJECT, identifier, NULL);
    ast->d.ptr = parseFile(filename);
    return ast;
}

void
ERROR(AST *instr, const char *msg, ...)
{
    va_list args;

    if (instr)
        fprintf(stderr, "%s:%d: error: ", instr->fileName, instr->line);
    else
        fprintf(stderr, "error: ");

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

    pp_init(&gl_pp);
    pp_setcomments(&gl_pp, "{", "}");
    initLexer();
}

static void
Usage(void)
{
    fprintf(stderr, "Spin to C++ converter version %s\n", VERSIONSTR);
    fprintf(stderr, "Usage: %s [options] file.spin\n", gl_progname);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --ccode:   output (static) C code instead of C++\n");
    fprintf(stderr, "  --dat:     output binary blob of DAT section only\n");
    fprintf(stderr, "  --elf:     create executable ELF file\n");
    fprintf(stderr, "  --files:   print list of .cpp files to stdout\n");
    fprintf(stderr, "  --main:    include C++ main() function\n");
    fprintf(stderr, "  --nopre:   do not run preprocessor on the .spin file\n"); 
    fprintf(stderr, "  -Dname=val: define a preprocessor symbol\n");
    exit(2);
}

#define MAX_CMDLINE 4096
static char cmdline[MAX_CMDLINE];

static void
appendWithoutSpace(const char *s)
{
    int len;
    len = strlen(cmdline) + strlen(s);
    if (len >= MAX_CMDLINE) {
        fprintf(stderr, "command line too long");
    }
    strcat(cmdline, s);
}

static void
appendToCmd(const char *s)
{
    if (cmdline[0] != 0)
        appendWithoutSpace(" ");
    appendWithoutSpace(s);
}

static void
appendCompiler(void)
{
    const char *ccompiler = getenv("CC");
    if (!ccompiler) ccompiler = "propeller-elf-gcc";
    appendToCmd(ccompiler);
}

int
main(int argc, char **argv)
{
    int outputMain = 0;
    int outputDat = 0;
    int outputFiles = 0;
    int outputElf = 0;
    int compile = 0;
    ParserState *P;
    int retval = 0;
    const char *cext = ".cpp";
    struct flexbuf argbuf;
    time_t timep;
    int i;

    init();

    /* save our command line arguments and comments describing
       how we were run
    */
    flexbuf_init(&argbuf, 128);
    flexbuf_addstr(&argbuf, "//\n// automatically generated by spin2cpp on ");
    time(&timep);
    flexbuf_addstr(&argbuf, asctime(localtime(&timep)));
    flexbuf_addstr(&argbuf, "// ");
    for (i = 0; i < argc; i++) {
        flexbuf_addstr(&argbuf, argv[i]);
        flexbuf_addchar(&argbuf, ' ');
    }
    flexbuf_addstr(&argbuf, "\n//\n\n");
    flexbuf_addchar(&argbuf, 0);
    gl_header = flexbuf_get(&argbuf);

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
        } else if (!strncmp(argv[0], "--main", 6) || !strcmp(argv[0], "-main")) {
            outputMain = 1;
            argv++; --argc;
        } else if (!strncmp(argv[0], "--dat", 5)) {
            outputDat = 1;
            argv++; --argc;
        } else if (!strncmp(argv[0], "--ccode", 7)) {
            gl_ccode = 1;
            gl_static = 1;
            cext = ".c";
            argv++; --argc;
        } else if (!strncmp(argv[0], "--files", 7)) {
            outputFiles = 1;
            argv++; --argc;
        } else if (!strncmp(argv[0], "--elf", 5)) {
            outputElf = compile = 1;
            outputMain = 1;
            argv++; --argc;
            appendCompiler();
        } else if (!strncmp(argv[0], "--nopre", 7)) {
            gl_preprocess = 0;
            argv++; --argc;
        } else if (!strncmp(argv[0], "--noheader", 9)) {
            free(gl_header);
            gl_header = NULL;
            argv++; --argc;
        } else if (!strncmp(argv[0], "-D", 2)) {
            char *opt = argv[0];
            char *name;
            argv++; --argc;
            if (opt[2] == 0) {
                if (argv[0] == NULL) {
                    fprintf(stderr, "Error: expected another argument after -D\n");
                    exit(2);
                }
                argv++; --argc;
                opt = argv[0];
            } else {
                opt += 2;
            }
            /* if we are compiling, pass this on to the compiler too */
            if (compile) {
                appendToCmd("-D");
                appendToCmd(opt);
            }
            opt = strdup(opt);
            name = opt;
            while (*opt && *opt != '=')
                opt++;
            if (*opt) {
                *opt++ = 0;
            } else {
                opt = "1";
            }
            pp_define(&gl_pp, name, opt);
        } else if (compile) {
            /* pass along arguments */
            if (!strncmp(argv[0], "-O", 2)
                || !strncmp(argv[0], "-f", 2)
                || !strncmp(argv[0], "-m", 2)
                || !strncmp(argv[0], "-g", 2)
                )
            {
                appendToCmd(argv[0]); argv++; --argc;
            } else if (!strncmp(argv[0], "-o", 2)
                       || !strncmp(argv[0], "-u", 2)
                       || !strncmp(argv[0], "-D", 2)
                )
            {
                char *opt = argv[0];
                appendToCmd(opt); argv++; --argc;
                if (opt[2] == 0) {
                    if (argv[0] == NULL) {
                        fprintf(stderr, "Error: expected another argument after %s option\n", opt);
                        exit(2);
                    }
                    appendToCmd(argv[0]); argv++; --argc;
                }
            } else {
                Usage();
            }
        } else {
            Usage();
        }
    }
    if (argv[0] == NULL || argc != 1) {
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
                    printf("%s%s\n", Q->basename, cext);
                }
                if (compile) {
                    appendToCmd(Q->basename);
                    appendWithoutSpace(cext);
                }
            }
            if (compile && gl_errors == 0) {
                retval = system(cmdline);
                if (retval < 0) {
                    fprintf(stderr, "Unable to run command: %s\n", cmdline);
                    exit(1);
                }
            }
        }
    } else {
        fprintf(stderr, "parse error\n");
        return 1;
    }

    if (gl_errors > 0) {
        fprintf(stderr, "%d errors\n", gl_errors);
        exit(1);
    }

    return retval;
}
