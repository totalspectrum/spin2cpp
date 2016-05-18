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
#include "version.h"

//#define DEBUG_YACC

extern int yyparse(void);

extern int yydebug;

Module *current;
Module *allparse;
Module *globalModule;

int gl_p2;
int gl_errors;
int gl_output;
int gl_outputflags;
int gl_nospin;
int gl_gas_dat;
int gl_normalizeIdents;
int gl_debug;
int gl_expand_constants;
int gl_optimize_flags;
int gl_dat_offset;
int gl_printprogress = 0;
int gl_depth = 0;
AST *ast_type_word, *ast_type_long, *ast_type_byte;
AST *ast_type_float, *ast_type_string;
AST *ast_type_generic;
AST *ast_type_void;

struct preprocess gl_pp;

// process a module after parsing it
static void ProcessModule(Module *P);

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
makeClassNameSafe(Module *P)
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
        char *newname = (char *)calloc(1, strlen(P->classname)+8);
        strcpy(newname, P->classname);
        free(P->classname);
        P->classname = newname;
        strcat(P->classname, "Class");
    }
}

/*
 * allocate a new parser state
 */ 
Module *
NewModule(const char *fullname)
{
    Module *P;
    char *s;
    char *root;

    P = (Module *)calloc(1, sizeof(*P));
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
    P->classname = (char *)calloc(1, strlen(P->basename)+1);
    strcpy(P->classname, P->basename);

    /* link the global symbols */
    if (globalModule) {
        P->objsyms.next = &globalModule->objsyms;
    } else {
        P->objsyms.next = &reservedWords;
    }
    return P;
}

/*
 * add global variable functions
 */

/* code for P1 */
const char p1_system_spincode[] =
    "pri waitcnt(x)\n"
    "  asm\n"
    "    waitcnt x,#0\n"
    "  endasm\n"
    "pri waitpeq(pin, mask, c)\n"
    "  asm\n"
    "    waitpeq pin,mask\n"
    "  endasm\n"
    "pri waitpne(pin, mask, c)\n"
    "  asm\n"
    "    waitpne pin,mask\n"
    "  endasm\n"
    "pri cogid | rval\n"
    "  asm\n"
    "    cogid rval\n"
    "  endasm\n"
    "  return rval\n"
    "pri cogstop(id)\n"
    "  asm\n"
    "    cogstop id\n"
    "  endasm\n"
    "  return 0\n"
    "pri clkset(mode, freq)\n"
    "  CLKFREQ := freq\n"
    "  CLKMODE := mode\n"
    "  asm\n"
    "    clkset mode\n"
    "  endasm\n"
    "pri reboot\n"
    "  clkset($80, 0)\n"
    "pri lockclr(id) | mask, rval\n"
    "  mask := -1\n"
    "  asm\n"
    "    lockclr id wc\n"
    "    muxc   rval,mask\n"
    "  endasm\n"
    "  return rval\n"
    "pri lockset(id) | mask, rval\n"
    "  mask := -1\n"
    "  asm\n"
    "    lockset id wc\n"
    "    muxc   rval,mask\n"
    "  endasm\n"
    "  return rval\n"
    "pri locknew | rval\n"
    "  asm\n"
    "    locknew rval\n"
    "  endasm\n"
    "  return rval\n"
    "pri lockret(id)\n"
    "  asm\n"
    "    lockret id\n"
    "  endasm\n"
    "  return 0\n"
    "pri longfill(ptr, val, count)\n"
    "  repeat count\n"
    "    long[ptr] := val\n"
    "    ptr += 4\n"
    "pri longmove(dst, src, count)\n"
    "  repeat count\n"
    "    long[dst] := long[src]\n"
    "    dst += 4\n"
    "    src += 4\n"
    "pri wordfill(ptr, val, count)\n"
    "  repeat count\n"
    "    word[ptr] := val\n"
    "    ptr += 2\n"
    "pri wordmove(dst, src, count)\n"
    "  repeat count\n"
    "    word[dst] := word[src]\n"
    "    dst += 2\n"
    "    src += 2\n"
    "pri bytefill(ptr, val, count)\n"
    "  repeat count\n"
    "    byte[ptr] := val\n"
    "    ptr += 1\n"
    "pri bytemove(dst, src, count)\n"
    "  repeat count\n"
    "    byte[dst] := byte[src]\n"
    "    dst += 1\n"
    "    src += 1\n"
    "pri strsize(str) : r\n"
    "  r := 0\n"
    "  repeat while byte[str] <> 0\n"
    "    r++\n"
    "    str++\n"
    "pri strcomp(s1, s2) | c1, c2\n"
    "  repeat\n"
    "    c1 := byte[s1++]\n"
    "    c2 := byte[s2++]\n"
    "    if (c1 <> c2)\n"
    "      return 0\n"
    "  until (c1 == 0)\n"
    "  return -1\n"
    "pri _coginit(id, code, param) | parm\n"
    "  parm := (param & $fffc) << 16\n"
    "  parm |= (code & $fffc) << 2\n"
    "  parm | = id & $f\n"
    "  asm\n"
    "    coginit parm wr\n"
    "  endasm\n"
    "  return parm\n"
    "pri _lookup(x, b, arr, n) | i\n"
    "  i := x - b\n"
    "  if (i => 0 and i < n)\n"
    "    return long[arr][i]\n"
    "  return 0\n"
    "pri _lookdown(x, b, arr, n) | i\n"
    "  repeat i from 0 to n-1\n"
    "    if (long[arr] == x)\n"
    "      return i+b\n"
    "    arr += 4\n"
    "  return 0\n"
    "pri _sqrt(a) | r, bit, tmp\n"
    "  if (a =< 0)\n"
    "    return 0\n"
    "  r := 0\n"
    "  bit := (1<<30)\n"
    "  repeat while (bit > a)\n"
    "    bit := bit >> 2\n"
    "  repeat while (bit <> 0)\n"
    "    tmp := r+bit\n"
    "    if (a => tmp)\n"
    "      a := a - tmp\n"
    "      r := (r >> 1) + bit\n"
    "    else\n"
    "      r := r >> 1\n"
    "    bit := bit >> 2\n"
    "  return r\n"
    "pri _lfsr_forward(x) | a\n"
    "  if (x == 0)\n"
    "    x := 1\n"
    "  a := $8000000b\n"
    "  repeat 32\n"
    "    asm\n"
    "      test x, a wc\n"
    "      rcl  x, #1\n"
    "    endasm\n"
    "  return x\n"
    "pri _lfsr_backward(x) | a\n"
    "  if (x == 0)\n"
    "    x := 1\n"
    "  a := $17\n"
    "  repeat 32\n"
    "    asm\n"
    "      test x, a wc\n"
    "      rcr  x, #1\n"
    "    endasm\n"
    "  return x\n"
;

// code for P2
// FIXME: there's a lot of duplication with P1
const char p2_system_spincode[] =
    "pri cnt | r\n"
    "  asm\n"
    "    getct r\n"
    "  endasm\n"
    "  return r\n"
    "pri waitcnt(x)\n"
    "  asm\n"
    "    addct1  x, #0\n"
    "    waitct1 x\n"
    "  endasm\n"
    "pri cogid | rval\n"
    "  asm\n"
    "    cogid rval\n"
    "  endasm\n"
    "  return rval\n"
    "pri cogstop(id)\n"
    "  asm\n"
    "    cogstop id\n"
    "  endasm\n"
    "  return 0\n"
    "pri lockclr(id) | mask, rval\n"
    "  mask := -1\n"
    "  asm\n"
    "    lockclr id wc\n"
    "    muxc   rval,mask\n"
    "  endasm\n"
    "  return rval\n"
    "pri lockset(id) | mask, rval\n"
    "  mask := -1\n"
    "  asm\n"
    "    lockset id wc\n"
    "    muxc   rval,mask\n"
    "  endasm\n"
    "  return rval\n"
    "pri locknew | rval\n"
    "  asm\n"
    "    locknew rval\n"
    "  endasm\n"
    "  return rval\n"
    "pri lockret(id)\n"
    "  asm\n"
    "    lockret id\n"
    "  endasm\n"
    "  return 0\n"
    "pri longfill(ptr, val, count)\n"
    "  repeat count\n"
    "    long[ptr] := val\n"
    "    ptr += 4\n"
    "pri longmove(dst, src, count)\n"
    "  repeat count\n"
    "    long[dst] := long[src]\n"
    "    dst += 4\n"
    "    src += 4\n"
    "pri wordfill(ptr, val, count)\n"
    "  repeat count\n"
    "    word[ptr] := val\n"
    "    ptr += 2\n"
    "pri wordmove(dst, src, count)\n"
    "  repeat count\n"
    "    word[dst] := word[src]\n"
    "    dst += 2\n"
    "    src += 2\n"
    "pri bytefill(ptr, val, count)\n"
    "  repeat count\n"
    "    byte[ptr] := val\n"
    "    ptr += 1\n"
    "pri bytemove(dst, src, count)\n"
    "  repeat count\n"
    "    byte[dst] := byte[src]\n"
    "    dst += 1\n"
    "    src += 1\n"
    "pri strsize(str) : r\n"
    "  r := 0\n"
    "  repeat while byte[str] <> 0\n"
    "    r++\n"
    "    str++\n"
    "pri strcomp(s1, s2) | c1, c2\n"
    "  repeat\n"
    "    c1 := byte[s1++]\n"
    "    c2 := byte[s2++]\n"
    "    if (c1 <> c2)\n"
    "      return 0\n"
    "  until (c1 == 0)\n"
    "  return -1\n"
    "pri clkset(mode, freq)\n"
    "  CLKFREQ := freq\n"
    "  CLKMODE := mode\n"
    "  asm\n"
    "    clkset mode\n"
    "  endasm\n"
    "pri reboot\n"
    "  clkset($80, 0)\n"
    "pri _lookup(x, b, arr, n) | i\n"
    "  i := x - b\n"
    "  if (i => 0 and i < n)\n"
    "    return long[arr][i]\n"
    "  return 0\n"
    "pri _lookdown(x, b, arr, n) | i\n"
    "  repeat i from 0 to n-1\n"
    "    if (long[arr] == x)\n"
    "      return i+b\n"
    "    arr += 4\n"
    "  return 0\n"
    "pri _sqrt(a) | r, bit, tmp\n"
    "  if (a =< 0)\n"
    "    return 0\n"
    "  r := 0\n"
    "  bit := (1<<30)\n"
    "  repeat while (bit > a)\n"
    "    bit := bit >> 2\n"
    "  repeat while (bit <> 0)\n"
    "    tmp := r+bit\n"
    "    if (a => tmp)\n"
    "      a := a - tmp\n"
    "      r := (r >> 1) + bit\n"
    "    else\n"
    "      r := r >> 1\n"
    "    bit := bit >> 2\n"
    "  return r\n"
    "pri _lfsr_forward(x) | a\n"
    "  if (x == 0)\n"
    "    x := 1\n"
    "  a := $8000000b\n"
    "  repeat 32\n"
    "    asm\n"
    "      test x, a wc\n"
    "      rcl  x, #1\n"
    "    endasm\n"
    "  return x\n"
    "pri _lfsr_backward(x) | a\n"
    "  if (x == 0)\n"
    "    x := 1\n"
    "  a := $17\n"
    "  repeat 32\n"
    "    asm\n"
    "      test x, a wc\n"
    "      rcr  x, #1\n"
    "    endasm\n"
    "  return x\n"
;

void
InitGlobalModule(void)
{
    SymbolTable *table;
    Symbol *sym;
    int oldtmpnum;
    const char *syscode;
    
    current = globalModule = NewModule("_system_");
    table = &globalModule->objsyms;
    sym = AddSymbol(table, "CLKFREQ", SYM_VARIABLE, ast_type_long);
    sym->flags |= SYMF_GLOBAL;
    sym->offset = gl_p2 ? 0x800 : 0;
    sym = AddSymbol(table, "CLKMODE", SYM_VARIABLE, ast_type_byte);
    sym->flags |= SYMF_GLOBAL;
    sym->offset = gl_p2 ? 0x804 : 4;

    /* compile inline assembly */
    if (gl_output == OUTPUT_ASM) {
        /* set up temporary variable processing */
        oldtmpnum = SetTempVariableBase(90000, 0);

        if (gl_p2) {
            syscode = p2_system_spincode;
        } else {
            syscode = p1_system_spincode;
        }
        strToLex(&globalModule->L, syscode, "_system_");
        yyparse();
        ProcessModule(globalModule);
        InferTypes(globalModule);
        ProcessFuncs(globalModule);
        SpinTransform(globalModule);
        CompileIntermediate(globalModule);

        curfunc = NULL;
        /* restore temp variable base */
        (void)SetTempVariableBase(oldtmpnum, 89999);
    }

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

static int
DeclareVariablesOfType(Module *P, AST *basetype, int offset)
{
    AST *upper;
    AST *ast;
    AST *curtype;
    int curtypesize;
    int basetypesize;
    
    basetypesize = TypeSize(basetype);
    
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
            curtypesize = 1;
            break;
        case AST_WORDLIST:
            curtype = ast_type_word;
            curtypesize = 2;
            break;
        case AST_LONGLIST:
            curtype = ast_type_generic;
            curtypesize = 4;
            break;
        case AST_COMMENT:
            /* skip */
            continue;
        default:
            ERROR(ast, "bad type  %d in variable list\n", ast->kind);
            return offset;
        }
        if (basetypesize == curtypesize) {
            offset = EnterVars(SYM_VARIABLE, &current->objsyms, curtype, ast->left, offset);
        }
    }
    return offset;
    
}

void
DeclareVariables(Module *P)
{
    int offset = 0;
    
    offset = DeclareVariablesOfType(P, ast_type_long, offset);
    offset = DeclareVariablesOfType(P, ast_type_word, offset);
    offset = DeclareVariablesOfType(P, ast_type_byte, offset);

    // round up to next LONG boundary
    offset = (offset + 3) & ~3;
    P->varsize = offset;
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
            AddSymbol(&current->objsyms, obj->d.string, SYM_OBJECT, ast);
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
 * recursively assign offsets to all objects in modules
 */
void
AssignObjectOffsets(Module *P)
{
    Module *Q;
    AST *ast, *obj;
    Symbol *sym = NULL;
    int offset;
    int count;
    Module *save=current;
    
    current = P;
    offset = P->varsize;
    for (ast = P->objblock; ast; ast = ast->right) {
        if (ast->kind != AST_OBJECT) {
            ERROR(ast, "Internal error: expected an OBJECT");
            return;
        }
        obj = ast->left;
        if (obj->kind == AST_IDENTIFIER) {
            sym = FindSymbol(&P->objsyms, obj->d.string);
            count = 1;
        } else if (obj->kind == AST_ARRAYDECL) {
            sym = FindSymbol(&P->objsyms, obj->left->d.string);
            count = EvalConstExpr(obj->right);
        }
        if (sym == NULL) {
            ERROR(ast, "Internal error, cannot find object symbol");
            return;
        }
        Q = GetObjectPtr(sym);
        AssignObjectOffsets(Q);
        sym->offset = offset;
        offset += count * Q->varsize;
    }
    offset = (offset+3) & ~3;
    P->varsize = offset;
    current = save;
}

/*
 * process a parsed module
 */
static void
ProcessModule(Module *P)
{
    P->botcomment = GetComments();

    /* now declare all the symbols that weren't already declared */
    DeclareConstants(&P->conblock);
    DeclareVariables(P);
    DeclareLabels(P);
    DeclareFunctions(P);
}

/*
 * parse a file
 * This is the main entry point for the compiler
 * "name" is the file name; if it has no .spin suffix
 * we'll try it with one
 * if "gl_depth" is >= 0 then print the file name
 */
Module *
ParseFile(const char *name)
{
    FILE *f = NULL;
    Module *P, *save, *Q, *LastQ;
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
    P = NewModule(fname);

    if (gl_printprogress) {
        int n = gl_depth;
        const char *tail;
        while (n > 0) {
            printf("|-");
            --n;
        }
        tail = FindLastDirectoryChar(fname);
        if (tail) {
            tail++;
        } else {
            tail = fname;
        }
        printf("%s\n", tail);
        gl_depth++;
    }
    
    /* if we have already visited an object with this name, skip it */
    /* also finds the last element in the list, so we can append to
       the list easily */
    LastQ = NULL;
    for (Q = allparse; Q; Q = Q->next) {
        if (!strcmp(P->basename, Q->basename)) {
            free(fname);
            free(P);
            fclose(f);
            if (gl_printprogress) {
                gl_depth--;
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

    if (gl_gas_dat) {
        current->datname = (char *)malloc(strlen(P->basename) + 40);
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
    if (gl_printprogress) {
        gl_depth--;
    }
    ProcessModule(P);

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
    ast->d.ptr = ParseFile(filename);
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

void
ERROR_UNKNOWN_SYMBOL(AST *ast)
{
    ERROR(ast, "Unknown symbol %s", ast->d.string);
    // add a definition for this symbol so we don't get this error again
    if (curfunc) {
        AddLocalVariable(curfunc, ast, SYM_LOCALVAR);
    }
}

void
Init()
{
    ast_type_long = NewAST(AST_INTTYPE, AstInteger(4), NULL);
    ast_type_word = NewAST(AST_UNSIGNEDTYPE, AstInteger(2), NULL);
    ast_type_byte = NewAST(AST_UNSIGNEDTYPE, AstInteger(1), NULL);
    ast_type_float = NewAST(AST_FLOATTYPE, AstInteger(4), NULL);
    ast_type_string = NewAST(AST_PTRTYPE, ast_type_byte, NULL);
    ast_type_generic = NewAST(AST_GENERICTYPE, AstInteger(4), NULL);
    ast_type_void = NewAST(AST_VOIDTYPE, AstInteger(0), NULL);
    initLexer(gl_p2);

    /* fill in the global symbol table */
    InitGlobalModule();
}

const char *
FindLastDirectoryChar(const char *fname)
{
    const char *found = NULL;
    if (!fname) return NULL;
    while (*fname) {
        if (*fname == '/'
#ifdef WIN32
            || *fname == '\\'
#endif
            )
        {
            found = fname;
        }
        fname++;
    }
    return found;
}

//
// use the directory portion of "directory" (if any) and then add
// on the basename
//
char *
ReplaceDirectory(const char *basename, const char *directory)
{
  char *ret = (char *)malloc(strlen(basename) + strlen(directory) + 2);
  char *dot;
  if (!ret) {
    fprintf(stderr, "FATAL: out of memory\n");
    exit(2);
  }
  strcpy(ret, directory);
  dot = (char *)FindLastDirectoryChar(ret);
  if (dot) {
      *dot++ = '/';
  } else {
      dot = ret;
  }
  strcpy(dot, basename);
  return ret;
}

char *
ReplaceExtension(const char *basename, const char *extension)
{
  char *ret = (char *)malloc(strlen(basename) + strlen(extension) + 1);
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
// may also pad the image out to form a .eeprom
// image, if eepromSize is non-zero
//
int
DoPropellerChecksum(const char *fname, size_t eepromSize)
{
    FILE *f = fopen(fname, "r+b");
    unsigned char checksum = 0;
    int c, r;
    size_t len;
    size_t padbytes;

    if (gl_p2) return 0; // no checksum required
    
    if (!f) {
        perror(fname);
        return -1;
    }
    fseek(f, 0L, SEEK_END);
    len = ftell(f);  // find length of file
    // pad file to multiple of 4, if necessary
    padbytes = ((len + 3) & ~3) - len;
    if (padbytes) {
        while (padbytes > 0) {
            fputc(0, f);
            padbytes--;
            len++;
        }
    }
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
    if (eepromSize && eepromSize >= len + 8) {
        fseek(f, 0L, SEEK_END);
        fputc(0xff, f); fputc(0xff, f);
        fputc(0xf9, f); fputc(0xff, f);
        fputc(0xff, f); fputc(0xff, f);
        fputc(0xf9, f); fputc(0xff, f);
        len += 8;
        while (len < eepromSize) {
            fputc(0, f);
            len++;
        }
    }
    fclose(f);
    return 0;
}

void InitPreprocessor()
{
    pp_init(&gl_pp);
    pp_setcomments(&gl_pp, "\'", "{", "}");
    pp_setlinedirective(&gl_pp, "{#line %d %s}");   
}
