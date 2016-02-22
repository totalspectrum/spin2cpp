/*
 * Spin to C/C++ translator
 * Copyright 2011-2016 Total Spectrum Software Inc.
 * 
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
int gl_gas_dat;
int gl_normalizeIdents;
int gl_debug;
int gl_expand_constants;
int gl_optimize_flags;
AST *ast_type_word, *ast_type_long, *ast_type_byte;
AST *ast_type_float, *ast_type_string;
AST *ast_type_generic;
AST *ast_type_void;

const char *gl_outname = NULL;

struct preprocess gl_pp;

int
yylex(YYSTYPE *yval)
{
    int c;
    c = getToken(&current->L, yval);
    if (c == T_EOF)
        return 0;
    return c;
}

const char *gl_progname = "spin2cpp";
char *gl_header = NULL;

static int
FindSymbolExact(SymbolTable *S, const char *name)
{
    Symbol *sym = FindSymbol(S, name);
    if (!sym)
        return 0;
    if (!strcmp(sym->name, name))
        return 1;
    return 0;
}

/*
 * make sure that a class name is safe, i.e. will
 * not conflict with any identifier or C keyword/function
 */
static void
makeClassNameSafe(ParserState *P)
{
    // check for conflict with C reserved word */
    if ( Is_C_Reserved(P->classname) 
         || (0 != FindSymbolExact(&P->objsyms, P->classname))
        )
    {
        NormalizeIdentifier(P->classname);
    }

    // now check for conflicts within the class
    while (0 != FindSymbolExact(&P->objsyms, P->classname)) {
        char *newname = calloc(1, strlen(P->classname)+8);
        strcpy(newname, P->classname);
        free(P->classname);
        P->classname = newname;
        strcat(P->classname, "Class");
    }
}

/*
 * allocate a new parser state
 */ 
ParserState *
NewParserState(const char *fullname)
{
    ParserState *P;
    char *s;
    char *root;

    P = calloc(1, sizeof(*P));
    if (!P) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    /* set up the base file name */
    P->fullname = fullname;
    P->basename = strdup(fullname);
    s = strrchr(P->basename, '.');
    if (s) *s = 0;
    root = strrchr(P->basename, '/');
#if defined(WIN32) || defined(WIN64)
    if (!root) {
      root = strrchr(P->basename, '\\');
    }
#endif
    if (!root)
      root = P->basename;
    else
      P->basename = root+1;
    /* set up the class name */
    P->classname = calloc(1, strlen(P->basename)+1);
    strcpy(P->classname, P->basename);
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
DeclareConstants(AST **conlist_ptr)
{
    AST *conlist;
    AST *upper, *ast, *id;
    AST *next;
    int default_val = 0;
    int n;

    conlist = *conlist_ptr;

    // first do all the simple assignments
    // this is necessary because Spin will sometimes allow out-of-order
    // assignments

    do {
        n = 0; // no assignments yet
        upper = conlist;
        while (upper) {
            next = upper->right;
            if (upper->kind == AST_LISTHOLDER) {
                ast = upper->left;
                if (ast->kind == AST_COMMENTEDNODE)
                    ast = ast->left;
                if (ast->kind == AST_ASSIGN) {
                    if (IsConstExpr(ast->right)) {
                        EnterConstant(ast->left->d.string, ast->right);
                        n++;
                        // now pull the assignment out so we don't see it again
                        RemoveFromList(conlist_ptr, upper);
                        upper->right = *conlist_ptr;
                        *conlist_ptr = upper;
                        conlist_ptr = &upper->right;
                        conlist = *conlist_ptr;
                    }
                }
            }
            upper = next;
        }

    } while (n > 0);

    for (upper = conlist; upper; upper = upper->right) {
        if (upper->kind == AST_LISTHOLDER) {
            ast = upper->left;
            if (ast->kind == AST_COMMENTEDNODE)
                ast = ast->left;
            switch (ast->kind) {
            case AST_ENUMSET:
                default_val = EvalConstExpr(ast->left);
                break;
            case AST_IDENTIFIER:
                EnterConstant(ast->d.string, AstInteger(default_val));
                default_val++;
                break;
            case AST_ENUMSKIP:
                id = ast->left;
                if (id->kind != AST_IDENTIFIER) {
                    ERROR(ast, "Internal error, expected identifier in constant list");
                } else {
                    EnterConstant(id->d.string, AstInteger(default_val));
                    default_val += EvalConstExpr(ast->right);
                }
                break;
            case AST_ASSIGN:
                EnterConstant(ast->left->d.string, ast->right);
                default_val = EvalConstExpr(ast->right) + 1;
                break;
            case AST_COMMENT:
                /* just skip it for now */
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
DeclareVariables(ParserState *P)
{
    AST *upper;
    AST *curtype;
    AST *ast;

    for (upper = P->varblock; upper; upper = upper->right) {
        if (upper->kind != AST_LISTHOLDER) {
            ERROR(upper, "Expected list holder\n");
        }
        ast = upper->left;
        if (ast->kind == AST_COMMENTEDNODE)
            ast = ast->left;
        switch (ast->kind) {
        case AST_BYTELIST:
            curtype = ast_type_byte;
            break;
        case AST_WORDLIST:
            curtype = ast_type_word;
            break;
        case AST_LONGLIST:
            curtype = ast_type_generic; // was ast_type_long;
            break;
        case AST_COMMENT:
            /* skip */
            continue;
        default:
            ERROR(ast, "bad type  %d in variable list\n", ast->kind);
            return;
        }
        EnterVars(SYM_VARIABLE, &current->objsyms, curtype, ast->left, 0);
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

/* helper function for parsing pasm FILE directives */
AST *
GetFullFileName(AST *baseString)
{
    const char *basename = baseString->d.string;
    char *newname;
    AST *ret;
    newname = find_file_on_path(&gl_pp, basename, NULL, current->fullname);
    ret = NewAST(AST_STRING, NULL, NULL);
    ret->d.string = newname;
    return ret;
}

/*
 * parse a file
 * This is the main entry point for the compiler
 * "name" is the file name; if it has no .spin suffix
 * we'll try it with one
 */
ParserState *
parseFile(const char *name)
{
    FILE *f = NULL;
    ParserState *P, *save, *Q, *LastQ;
    char *fname = NULL;
    char *parseString = NULL;

    if (current)
      fname = find_file_on_path(&gl_pp, name, ".spin", current->fullname);
    if (!fname)
      fname = strdup(name);

    f = fopen(fname, "r");
    if (!f) {
        perror(name);
        free(fname);
        exit(1);
    }
    save = current;
    P = NewParserState(fname);

    /* if we have already visited an object with this name, skip it */
    /* also finds the last element in the list, so we can append to
       the list easily */
    LastQ = NULL;
    for (Q = allparse; Q; Q = Q->next) {
        if (!strcmp(P->basename, Q->basename)) {
            free(fname);
            free(P);
            fclose(f);
            return Q;
        }
        LastQ = Q;
    }
    if (LastQ)
        LastQ->next = P;
    else
        allparse = P;
    current = P;

    if (gl_gas_dat) {
        current->datname = malloc(strlen(P->basename) + 40);
        if (!current->datname) {
            fprintf(stderr, "Out of memory!\n");
            exit(2);
        }
        sprintf(current->datname, "_load_start_%s_cog", P->basename);
    } else {
        current->datname = "dat";
    }
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
        free(fname);
        exit(1);
    }
    P->botcomment = GetComments();

    /* now declare all the symbols that weren't already declared */
    DeclareConstants(&P->conblock);
    DeclareVariables(P);
    DeclareLabels(P);
    DeclareFunctions(P);

    /* work to avoid conflicts with variables and constants */
    makeClassNameSafe(P);

    if (gl_errors > 0) {
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

void
WARNING(AST *instr, const char *msg, ...)
{
    va_list args;

    if (instr)
        fprintf(stderr, "%s:%d: warning: ", instr->fileName, instr->line);
    else
        fprintf(stderr, "warning: ");

    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    fprintf(stderr, "\n");
}

static void
init()
{
    ast_type_long = NewAST(AST_INTTYPE, AstInteger(4), NULL);
    ast_type_word = NewAST(AST_UNSIGNEDTYPE, AstInteger(2), NULL);
    ast_type_byte = NewAST(AST_UNSIGNEDTYPE, AstInteger(1), NULL);
    ast_type_float = NewAST(AST_FLOATTYPE, AstInteger(4), NULL);
    ast_type_string = NewAST(AST_PTRTYPE, ast_type_byte, NULL);
    ast_type_generic = NewAST(AST_GENERICTYPE, AstInteger(4), NULL);
    ast_type_void = NewAST(AST_VOIDTYPE, AstInteger(0), NULL);
    initLexer(0);
}

static void
Usage(void)
{
    fprintf(stderr, "Spin to C++ converter version %s\n", VERSIONSTR);
    fprintf(stderr, "Usage: %s [options] file.spin\n", gl_progname);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --asm:     output (user readable) PASM code\n");
    fprintf(stderr, "  --binary:  create binary file for download\n");
    fprintf(stderr, "  --ccode:   output (static) C code instead of C++\n");
    fprintf(stderr, "  --dat:     output binary blob of DAT section only\n");
    fprintf(stderr, "  --elf:     create executable ELF file with propgcc\n");
    fprintf(stderr, "  --catalina: convert to C and run Catalina on result\n");
    fprintf(stderr, "  --files:   print list of .cpp files to stdout\n");
    fprintf(stderr, "  --gas:     create inline assembly out of DAT area;\n");
    fprintf(stderr, "             with --dat, create gas .S file from DAT area\n");
    fprintf(stderr, "  --main:    include C++ main() function\n");
    fprintf(stderr, "  --nopre:   do not run preprocessor on the .spin file\n"); 
    fprintf(stderr, "  --normalize: normalize case of all identifiers\n"); 
    fprintf(stderr, "  -Dname=val: define a preprocessor symbol\n");
    fprintf(stderr, "  -I dir:     add dir to the object search path\n");
    exit(2);
}

#define MAX_CMDLINE 4096
static char cmdline[MAX_CMDLINE];

static void
initCmdline(void)
{
  cmdline[0] = 0;
}

#define needsquote(x) (needEscape && ((x) == ' '))

static void
appendWithoutSpace(const char *s, int needEscape)
{
    int len = 0;
    const char *src = s;
    char *dst = cmdline;
    int c;
    int addquote = 0;

    // move dst to the end of cmdline, and count up
    // the size
    while (*dst) {
      dst++;
      len++;
    }
    // check to see if "s" contains any spaces; if so,
    // we will have to escape those
    while (*s) {
      if (needsquote(*s))
	addquote = 1;
      s++;
      len++;
    }
    if (addquote) len += 2;
    if (len >= MAX_CMDLINE) {
        fprintf(stderr, "command line too long: aborting");
	exit(2);
    }
    // now actually copy it in
    if (addquote)
      *dst++ = '"';
    do {
      c = *src++;
      if (!c) break;
      *dst++ = c;
    } while (c);
    if (addquote)
      *dst++ = '"';
    *dst++ = 0;
}

static void
appendToCmd(const char *s)
{
    if (cmdline[0] != 0)
      appendWithoutSpace(" ", 0);
    appendWithoutSpace(s, 1);
}

static void
appendCompiler(const char *ccompiler)
{
    if (!ccompiler) {
        ccompiler = getenv("CC");
    }
    if (!ccompiler) {
        ccompiler = "propeller-elf-gcc";
    }
    appendToCmd(ccompiler);
}

char *
ReplaceExtension(const char *basename, const char *extension)
{
  char *ret = malloc(strlen(basename) + strlen(extension) + 1);
  char *dot;
  if (!ret) {
    fprintf(stderr, "FATAL: out of memory\n");
    exit(2);
  }
  strcpy(ret, basename);
  dot = strrchr(ret, '/');
  if (!dot) dot = ret;
  dot = strrchr(dot, '.');
  if (dot) *dot = 0;
  strcat(ret, extension);
  return ret;
}

//
// add a propeller checksum to a binary file
//
int
DoPropellerChecksum(const char *fname)
{
    FILE *f = fopen(fname, "r+b");
    unsigned char checksum = 0;
    int c, r;
    size_t len;

    if (!f) {
        perror(fname);
        return -1;
    }
    fseek(f, 0L, SEEK_END);
    len = ftell(f);  // find length of file
    // update header fields
    fseek(f, 8L, SEEK_SET); // seek to 16 bit vbase field
    fputc(len & 0xff, f);
    fputc( (len >> 8) & 0xff, f);
    // update dbase = vbase + 2 * sizeof(int)
    fputc( (len+8) & 0xff, f);
    fputc( ((len+8)>>8) & 0xff, f);
    // update initial program counter
    fputc( 0x18, f );
    fputc( 0x00, f );
    // update dcurr = dbase + 2 * sizeof(int)
    fputc( (len+16) & 0xff, f);
    fputc( ((len+16)>>8) & 0xff, f);

    fseek(f, 0L, SEEK_SET);
    for(;;) {
        c = fgetc(f);
        if (c < 0) break;
        checksum += (unsigned char)c;
    }
    fflush(f);
    checksum = 0x14 - checksum;
    r = fseek(f, 5L, SEEK_SET);
    if (r != 0) {
        perror("fseek");
        return -1;
    }
    //printf("writing checksum 0x%x\n", checksum);
    r = fputc(checksum, f);
    if (r < 0) {
        perror("fputc");
        return -1;
    }
    fflush(f);
    fclose(f);
    return 0;
}

int
main(int argc, char **argv)
{
    int outputMain = 0;
    int outputDat = 0;
    int outputFiles = 0;
    int outputBin = 0;
    int outputAsm = 0;
    int compile = 0;
    ParserState *P;
    int retval = 0;
    const char *cext = ".cpp";
    struct flexbuf argbuf;
    time_t timep;
    int i;
    const char *outname = NULL;

    /* Initialize the global preprocessor; we need to do this here
       so that the -D command line option can define preprocessor
       symbols. The rest of initialization happens after command line
       options have been parsed
    */
    pp_init(&gl_pp);
    pp_setcomments(&gl_pp, "\'", "{", "}");
    pp_setlinedirective(&gl_pp, "{#line %d %s}");

    /* save our command line arguments and comments describing
       how we were run
    */
    flexbuf_init(&argbuf, 128);
    flexbuf_addstr(&argbuf, "//\n// automatically generated by spin2cpp v" VERSIONSTR " on ");
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
        } else if (!strncmp(argv[0], "--dat", 5) || (!compile && !strcmp(argv[0], "-c"))) {
            outputDat = 1;
            argv++; --argc;
        } else if (!strncmp(argv[0], "--noopt", 5) ) {
            gl_optimize_flags |= OPT_NO_ASM;
            argv++; --argc;
        } else if (!strncmp(argv[0], "--asm", 5) ) {
            outputAsm = 1;
            argv++; --argc;
        } else if (!strncmp(argv[0], "--gas", 5)) {
            gl_gas_dat = 1;
            argv++; --argc;
        } else if (!strncmp(argv[0], "--normalize", 8) || !strcmp(argv[0], "-n")) {
            gl_normalizeIdents = 1;
            argv++; --argc;
        } else if (!strncmp(argv[0], "--ccode", 7)) {
            gl_ccode = 1;
            cext = ".c";
            argv++; --argc;
        } else if (!strncmp(argv[0], "--optimize", 5)) {
            argv++; --argc;
            if (argv[0] == NULL) {
                fprintf(stderr, "Error: expected another argument after --optimize\n");
                exit(2);
            }
            gl_optimize_flags = strtoul(argv[0], NULL, 0);
            argv++; --argc;
        } else if (!strncmp(argv[0], "--files", 7)) {
            outputFiles = 1;
            argv++; --argc;
        } else if (!strncmp(argv[0], "--elf", 5)) {
            compile = 1;
            outputMain = 1;
            argv++; --argc;
            appendCompiler(NULL);
            gl_optimize_flags |= OPT_REMOVE_UNUSED_FUNCS;
        } else if (!strncmp(argv[0], "--catalina", 10)) {
            compile = 1;
            outputMain = 1;
            gl_ccode = 1;
            cext = ".c";
            argv++; --argc;
            appendCompiler("catalina");
        } else if (!strncmp(argv[0], "--bin", 5) || !strcmp(argv[0], "-b")) {
            compile = 1;
            outputMain = 1;
	    outputBin = 1;
            argv++; --argc;
            gl_optimize_flags |= OPT_REMOVE_UNUSED_FUNCS;
            appendCompiler(NULL);
        } else if (!strncmp(argv[0], "--nopre", 7)) {
            gl_preprocess = 0;
            argv++; --argc;
        } else if (!strncmp(argv[0], "--noheader", 9)) {
            free(gl_header);
            gl_header = NULL;
            argv++; --argc;
	} else if (!strncmp(argv[0], "-o", 2)) {
	    char *opt;
	    opt = argv[0];
            argv++; --argc;
            if (opt[2] == 0) {
                if (argv[0] == NULL) {
                    fprintf(stderr, "Error: expected another argument after -o\n");
                    exit(2);
                }
                opt = argv[0];
		argv++; --argc;
            } else {
                opt += 2;
            }
	    gl_outname = strdup(opt);
        } else if (!strncmp(argv[0], "-g", 2)) {
            if (compile) {
                appendToCmd(argv[0]);
            }
            argv++; --argc;
            gl_debug = 1;
        } else if (!strncmp(argv[0], "-D", 2) || !strncmp(argv[0], "-C", 2)) {
            char *opt = argv[0];
            char *name;
            char optchar[3];
            argv++; --argc;
            // save the -D or -C
            strncpy(optchar, opt, 2);
            optchar[2] = 0;
            if (opt[2] == 0) {
                if (argv[0] == NULL) {
                    fprintf(stderr, "Error: expected another argument after %s\n", optchar);
                    exit(2);
                }
                opt = argv[0];
                argv++; --argc;
            } else {
                opt += 2;
            }
            /* if we are compiling, pass this on to the compiler too */
            if (compile) {
                appendToCmd(optchar);
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
        } else if (!strncmp(argv[0], "-L", 2) || !strncmp(argv[0], "-I", 2)) {
            char *opt = argv[0];
            char *incpath;
            char optchar[3];
            argv++; --argc;
            // save the -L or -I
            strncpy(optchar, opt, 2);
            optchar[2] = 0;
            if (opt[2] == 0) {
                if (argv[0] == NULL) {
                    fprintf(stderr, "Error: expected another argument after %s\n", optchar);
                    exit(2);
                }
                opt = argv[0];
                argv++; --argc;
            } else {
                opt += 2;
            }
            /* if we are compiling, pass this on to the compiler too */
            if (compile) {
                appendToCmd(optchar);
                appendToCmd(opt);
            }
            opt = strdup(opt);
            incpath = opt;
            pp_add_to_path(&gl_pp, incpath);
        } else if (compile) {
            /* pass along arguments */
            if (!strncmp(argv[0], "-O", 2)
                || !strncmp(argv[0], "-f", 2)
                || !strncmp(argv[0], "-m", 2)
                || !strncmp(argv[0], "-l", 2)
                )
            {
                appendToCmd(argv[0]); argv++; --argc;
            } else if (!strncmp(argv[0], "-u", 2)
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
    if (argv[0] == NULL || (argc != 1 && !compile)) {
        Usage();
    }

    /* initialize the parser; we do that after command line processing
       so that command line options can influence it */
    init();

    /* now actually parse the file */
    P = parseFile(argv[0]);
    if (compile && argc > 1) {
        /* append the remaining arguments to the command line */
        for (i = 1; i < argc; i++) {
            appendToCmd(argv[i]);
        }
    }

    if (P) {
        /* do type checking and deduction */
        int changes;
        ParserState *Q;
        MarkUsed(P->functions);
        for (Q = allparse; Q; Q = Q->next) {
            ProcessFuncs(Q);
        }
        do {
            changes = 0;
            for (Q = allparse; Q; Q = Q->next) {
                changes += InferTypes(Q);
            }
        } while (changes != 0);
        if (gl_errors > 0) {
            exit(1);
        }
        for (Q = allparse; Q; Q = Q->next) {
            SpinTransform(Q);
        }
        if (outputDat) {
            outname = gl_outname;
            if (gl_gas_dat) {
	        if (!outname) {
                    outname = ReplaceExtension(P->basename, ".S");
                }
                OutputGasFile(outname, P);
            } else {
	        if (!outname) {
                    if (outputBin) {
                        outname = ReplaceExtension(P->basename, ".binary");
                    } else {
                        outname = ReplaceExtension(P->basename, ".dat");
                    }
                }
                OutputDatFile(outname, P, outputBin);
                if (outputBin) {
                    DoPropellerChecksum(outname);
                }
            }
        } else if (outputAsm) {
            if (!outname) {
                outname = ReplaceExtension(P->basename, ".pasm");
            }
            OutputAsmCode(outname, P);
        } else {
            /* compile any sub-objects needed */
            for (Q = allparse; Q; Q = Q->next) {
                OutputCppCode(Q->basename, Q, outputMain && Q == P);
                if (outputFiles) {
                    printf("%s%s\n", Q->basename, cext);
                }
                if (compile) {
                    appendToCmd(Q->basename);
                    appendWithoutSpace(cext, 1);
                }
            }
            if (compile && gl_errors == 0) {
                const char *binname;
                const char *elfname;

                if (gl_outname == NULL) {
                    elfname = ReplaceExtension(argv[0], ".elf");
                    binname = ReplaceExtension(elfname, ".binary");
                } else if (outputBin) {
                    elfname = ReplaceExtension(gl_outname, ".elf");
                    binname = gl_outname;
                } else {
                    elfname = gl_outname;
                    binname = NULL;
                }
                appendToCmd("-o");
                appendToCmd(elfname);

                retval = system(cmdline);
                if (retval < 0) {
                    fprintf(stderr, "Unable to run command: %s\n", cmdline);
                    exit(1);
                }
		if (retval == 0 && outputBin) {
                    initCmdline();
                    appendToCmd("propeller-elf-objcopy");
                    appendToCmd("-O");
                    appendToCmd("binary");
                    appendToCmd(elfname);
                    appendToCmd(binname);
                    //printf("running: [%s]\n", cmdline);

                    retval = system(cmdline);
                    remove(elfname);
                    if (retval == 0) {
                        retval = DoPropellerChecksum(binname);
                    }
		}
		if (retval != 0)
		  gl_errors++;
            }
        }
    } else {
        fprintf(stderr, "parse error\n");
        return 1;
    }

    if (gl_errors > 0) {
        exit(1);
    }

    return retval;
}
