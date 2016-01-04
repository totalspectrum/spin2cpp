//
// C++ source code output for spin2cpp
//
// Copyright 2012-2015 Total Spectrum Software Inc.
// see the file COPYING for conditions of redistribution
//

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <ctype.h>
#include "spinc.h"


/*
 * print out a declaration for the dat[] array
 * "tail" is appended to the declaration
 * if "classname" is TRUE, then add the class name as well
 */
static void
PrintDatArray(FILE *f, ParserState *parse, const char *tail, bool classname)
{
    char *datname = parse->datname;

    fprintf(f, "uint8_t ");
    if (parse->datannotations) {
        PrintAnnotationList(f, parse->datannotations, ' ');
    }
    if (classname) {
        fprintf(f, "%s::", parse->classname);
    }
    fprintf(f, "%s[]%s", datname, tail);
}

/*
 * print out a comment string
 */
static void
PrintCommentString(FILE *f, const char *str, int indent)
{
    int numLines = 0;
    int lastNewLine = 0;
    const char *p;
    for (p = str; *p; p++) {
        if (*p =='\n') {
            numLines++; lastNewLine = 1;
        } else {
            lastNewLine = 0;
        }
    }
    if (indent > 0)
        fprintf(f, "%*c", indent, ' ');
    if (numLines == 1 && lastNewLine == 1) {
        fprintf(f, "//");
        // C++ comments look much better with a space after the //; Spin
        // comments don't matter so much, so people leave them out.
        // optionally add the space
        if (!isspace(*str)) {
            fprintf(f, " ");
        }
        fprintf(f, "%s", str);  // note the string ended in a newline
        return;
    }
    fprintf(f, "/* ");
    while (str[0]) {
        if (str[0] == '*' && str[1] == '/') {
            fputc('*', f);
            fputc('%', f);
            str += 2;
        } else {
            fputc(str[0], f);
            lastNewLine = (str[0] == '\n');
            if (lastNewLine && indent > 0) {
                fprintf(f, "%*c", indent, ' ');
            }
            str++;
        }
    }
    if (numLines > 0 && !lastNewLine) {
        fprintf(f, "\n");
        if (indent > 0)
            fprintf(f, "%*c", indent, ' ');
    }
    fprintf(f, " */\n");
}


/*
 * print out a list of comments
 */
void
PrintIndentedComment(FILE *f, AST *ast, int indent)
{
    while (ast) {
        if (ast->kind != AST_COMMENT) {
            ERROR(ast, "Internal error: expected comment");
            return;
        }
        PrintCommentString(f, ast->d.string, indent);
        ast = ast->right;
    }
}

/*
 * print out a header file
 */

/*
 * print constant declarations
 */
static void
PrintConstant(FILE *f, AST *ast)
{
    if (IsFloatConst(ast)) {
        PrintFloat(f, EvalConstExpr(ast));
        return;
    }
    PrintExpr(f, ast);
}

static void
PrintConstantDecl(FILE *f, AST *ast)
{
    const char *name = ast->d.string;
    AST *expr = NULL;
    Symbol *sym;

    if (!IsConstExpr(ast)) {
        ERROR(ast, "%s is not constant", name);
        return;
    }
    sym = LookupSymbol(name);
    if (!sym) {
        ERROR(ast, "constant symbol %s not declared??", name);
        return;
    }
    expr = (AST *)sym->val;
    if (gl_ccode) {
        fprintf(f, "#define %s (", ast->d.string);
        PrintConstant(f, expr);
        fprintf(f, ")\n");
    } else {
        fprintf(f, "  static const int %s = ", ast->d.string);
        PrintConstant(f, expr);
        fprintf(f, ";\n");
    }
}

static int
PrintAllVarListsOfType(FILE *f, ParserState *parse, AST *type, int flags)
{
    AST *ast;
    AST *upper;
    AST *comment;
    enum astkind kind;
    int n = 0;

    if (type == ast_type_byte)
        kind = AST_BYTELIST;
    else if (type == ast_type_word)
        kind = AST_WORDLIST;
    else
        kind = AST_LONGLIST;
    
    for (upper = parse->varblock; upper; upper = upper->right) {
        if (upper->kind != AST_LISTHOLDER) {
            ERROR(upper, "internal error: expected listholder");
            return n;
        }
        ast = upper->left;
        comment = NULL;
        if (ast->kind == AST_COMMENTEDNODE) {
            comment = ast->right;
            ast = ast->left;
        }
        if (ast->kind == kind) {
            if (comment)
                PrintComment(f, comment);
            n += PrintVarList(f, type, ast->left, flags);
        }
    }
    return n;
}

static void
PrintSubHeaders(FILE *f, ParserState *parse)
{
    AST *ast, *sub;
    ParserState *objstate;
    int already_done;

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

}

static void
PrintAllConstants(FILE *f, ParserState *parse)
{
    AST *ast, *upper;

    /* print the constant declarations */
    for (upper = parse->conblock; upper; upper = upper->right) {
        ast = upper->left;
        if (ast->kind == AST_COMMENTEDNODE) {
            PrintComment(f, ast->right);
            ast = ast->left;
        }
        switch (ast->kind) {
        case AST_ENUMSKIP:
            PrintConstantDecl(f, ast->left);
            break;
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
}

#if 0
static void
PrintObjectRefs(FILE *f, ParserState *parse)
{
}
#endif

static void
PrintCHeaderFile(FILE *f, ParserState *parse)
{
    int n;
    AST *ast;
    int flags = PRIVATE;

    /* things we always need */
    if (gl_header) {
        fprintf(f, "%s", gl_header);
    }
    fprintf(f, "#ifndef %s_Class_Defined__\n", parse->classname);
    fprintf(f, "#define %s_Class_Defined__\n\n", parse->classname);

    fprintf(f, "#include <stdint.h>\n");

    PrintSubHeaders(f, parse);
    PrintAllConstants(f, parse);

    if (parse->volatileVariables)
        flags |= VOLATILE;

    /* print the structure definition */
    fprintf(f, "\ntypedef struct %s {\n", parse->classname);
    n = PrintAllVarListsOfType(f, parse, ast_type_long, flags);
    n += PrintAllVarListsOfType(f, parse, ast_type_word, flags);
    n += PrintAllVarListsOfType(f, parse, ast_type_byte, flags);

    /* object references */
    for (ast = parse->objblock; ast; ast = ast->right) {
        ParserState *P = ast->d.ptr;
        AST *objdef = ast->left;
        if (objdef->kind == AST_IDENTIFIER) {
            fprintf(f, "  %s\t%s;\n", P->classname, objdef->d.string);
            n++;
        } else if (objdef->kind == AST_ARRAYDECL) {
            AST *arrname = objdef->left;
            AST *arrsize = objdef->right;
            if (arrname->kind == AST_IDENTIFIER) {
                fprintf(f, "  %s\t%s[%d];\n", P->classname,
                        arrname->d.string,
                        EvalConstExpr(arrsize)
                    );
                n++;
            } else {
                ERROR(objdef, "internal error in object printing");
            }
        }
    }

    /* needed to avoid problems with empty structures on Catalina */
    if (n == 0)
        fprintf(f, "  char dummy__;\n");
    fprintf(f, "} %s;\n\n", parse->classname);

    /* now the public members */
    PrintPublicFunctionDecls(f, parse);

    /* now the private methods */
    /* PrintPrivateFunctionDecls(f, parse); */

    fprintf(f, "#endif\n");
}

static void
PrintCppHeaderFile(FILE *f, ParserState *parse)
{
    AST *ast;
    int flags = PRIVATE;

    /* things we always need */
    if (gl_header) {
        fprintf(f, "%s", gl_header);
    }
    fprintf(f, "#ifndef %s_Class_Defined__\n", parse->classname);
    fprintf(f, "#define %s_Class_Defined__\n\n", parse->classname);

    fprintf(f, "#include <stdint.h>\n");

    /* include any needed object headers */
    PrintSubHeaders(f, parse);

    /* print the constant declarations */
    fprintf(f, "class %s {\npublic:\n", parse->classname);

    PrintAllConstants(f, parse);

    /* object references */
    for (ast = parse->objblock; ast; ast = ast->right) {
        ParserState *P = ast->d.ptr;
        AST *objdef = ast->left;
        if (objdef->kind == AST_IDENTIFIER) {
            fprintf(f, "  %s\t%s;\n", P->classname, objdef->d.string);
        } else if (objdef->kind == AST_ARRAYDECL) {
            AST *arrname = objdef->left;
            AST *arrsize = objdef->right;
            if (arrname->kind == AST_IDENTIFIER) {
                fprintf(f, "  %s\t%s[%d];\n", P->classname,
                        arrname->d.string,
                        EvalConstExpr(arrsize)
                    );
            } else {
                ERROR(objdef, "internal error in object printing");
            }
        }
    }
    /* data block, if applicable */
    if (parse->datblock && !gl_gas_dat) {
        fprintf(f, "  static ");
        PrintDatArray(f, parse, ";\n", false);
    }
    /* now the public members */
    PrintPublicFunctionDecls(f, parse);

    /* now the private members */
    /* Note that Spin sorts these, outputing first the 32 bit
       vars, then 16, finally 8
    */
    if (parse->volatileVariables)
        flags |= VOLATILE;
    fprintf(f, "private:\n");
    PrintAllVarListsOfType(f, parse, ast_type_long, flags);
    PrintAllVarListsOfType(f, parse, ast_type_word, flags);
    PrintAllVarListsOfType(f, parse, ast_type_byte, flags);

    /* now the private methods */
    PrintPrivateFunctionDecls(f, parse);
    fprintf(f, "};\n\n");
    fprintf(f, "#endif\n");
}

static void
PrintMacros(FILE *f, ParserState *parse)
{
    if (gl_nospin)
        return;

    fprintf(f, "#ifdef __GNUC__\n");
    fprintf(f, "#define INLINE__ static inline\n");
    if (parse->needsYield) {
        // old way
        //fprintf(f, "#include <sys/thread.h>\n");
        //fprintf(f, "#define Yield__() (__napuntil(_CNT))\n");
        // new way -- not as thread friendly, but much faster
        fprintf(f, "#define Yield__() __asm__ volatile( \"\" ::: \"memory\" )\n");
    }
    if (parse->needsHighmult) {
        fprintf(f, "#define Highmult__(X, Y) ( ( (X) * (int64_t)(Y) ) >> 32 )\n");
    }
    if (parse->needsBitEncode) {
        fprintf(f, "#define BitEncode__(X) (32 - __builtin_clz(X))\n");
    }
    fprintf(f, "#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })\n");

    fprintf(f, "#else\n");

    fprintf(f, "#define INLINE__ static\n");
    fprintf(f, "static int32_t tmp__;\n");
    fprintf(f, "#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)\n");

    if (parse->needsYield) {
        fprintf(f, "#define Yield__()\n");
    }
    if (gl_ccode) {
        fprintf(f, "#define waitcnt(n) _waitcnt(n)\n");
        if (parse->needsLockFuncs) {
            fprintf(f, "#define locknew() _locknew()\n");
            fprintf(f, "#define lockret(i) _lockret(i)\n");
            fprintf(f, "#define lockset(i) _lockset(i)\n");
            fprintf(f, "#define lockclr(i) _lockclr(i)\n");
        }
        fprintf(f, "#define coginit(id, code, par) _coginit((unsigned)(par)>>2, (unsigned)(code)>>2, id)\n");
        fprintf(f, "#define cognew(code, par) coginit(0x8, (code), (par))\n");
        fprintf(f, "#define cogstop(i) _cogstop(i)\n");
        if (parse->needsHighmult) {
            fprintf(f, "static int32_t Highmult__(int32_t a, int32_t b) {\n");
            fprintf(f, "  int sign = (a^b)>>31;\n");
            fprintf(f, "  uint32_t ua = a < 0 ? -a : a;\n");
            fprintf(f, "  uint32_t ub = b < 0 ? -b : b;\n");
            fprintf(f, "  uint32_t rhi = 0, rlo = 0;\n");
            fprintf(f, "  int i;\n");
            fprintf(f, "  for (i = 0; i < 32; i++) {\n");
            fprintf(f, "    rhi = (rhi << 1) | (rlo >> 31);\n");
            fprintf(f, "    rlo = rlo << 1;\n");
            fprintf(f, "    if (ua >> 31) {rlo += ub; if (rlo < ub) rhi++;}\n");
            fprintf(f, "    ua = ua<<1;\n");
            fprintf(f, "  }\n");
            fprintf(f, "  if (sign) { rhi = -rhi; rhi -= (rlo != 0); }\n");
            fprintf(f, "  return rhi;\n");
            fprintf(f, "}\n");
        }
        if (parse->needsBitEncode) {
            fprintf(f, "INLINE__ int32_t BitEncode__(uint32_t a) {\n");
            fprintf(f, "  int r=0;\n");
            fprintf(f, "  while (a != 0) { a = a>>1; r++; }\n");
            fprintf(f, "  return r;\n");
            fprintf(f, "}\n");
        }
    }
    fprintf(f, "#endif\n");
    fprintf(f, "\n");
    if (parse->needsMinMax) {
        fprintf(f, "INLINE__ int32_t Min__(int32_t a, int32_t b) { return a < b ? a : b; }\n"); 
        fprintf(f, "INLINE__ int32_t Max__(int32_t a, int32_t b) { return a > b ? a : b; }\n"); 
    }
    if (parse->needsAbortdef) {
        if (!gl_ccode)
            fprintf(f, "extern \"C\" {\n");
        fprintf(f, "#include <setjmp.h>\n");
        if (!gl_ccode)
            fprintf(f, "}\n");
        fprintf(f, "typedef struct { jmp_buf jmp; int32_t val; } AbortHook__;\n");
        fprintf(f, "AbortHook__ *abortChain__ __attribute__((common));\n\n");
    }
    if (parse->needsRotate) {
        fprintf(f, "INLINE__ int32_t Rotl__(uint32_t a, uint32_t b) { return (a<<b) | (a>>(32-b)); }\n"); 
        fprintf(f, "INLINE__ int32_t Rotr__(uint32_t a, uint32_t b) { return (a>>b) | (a<<(32-b)); }\n"); 
    }
    if (parse->needsShr) {
        fprintf(f, "INLINE__ int32_t Shr__(uint32_t a, uint32_t b) { return (a>>b); }\n"); 
    }
    if (parse->needsBetween) {
        fprintf(f, "INLINE__ int32_t Between__(int32_t x, int32_t a, int32_t b){ if (a <= b) return x >= a && x <= b; return x >= b && x <= a; }\n\n");
    }
    if (parse->needsLookup) {
        fprintf(f, "INLINE__ int32_t Lookup__(int32_t x, int32_t b, int32_t a[], int32_t n) { int32_t i = (x)-(b); return ((unsigned)i >= n) ? 0 : (a)[i]; }\n");
        fprintf(f, "\n");
    }
    if (parse->needsLookdown) {
        fprintf(f, "INLINE__ int32_t Lookdown__(int32_t x, int32_t b, int32_t a[], int32_t n) { int32_t i, r; r = 0; for (i = 0; i < n; i++) { if (a[i] == x) { r = i+b; break; } }; return r; }\n");
        fprintf(f, "\n");
    }
    if (parse->needsRand) {
        fprintf(f, "static uint32_t LFSR__(uint32_t x, uint32_t forward) {\n");
        fprintf(f, "    uint32_t y, c, a;\n");
        fprintf(f, "    if (x < 1) x = 1;\n");
        fprintf(f, "    a = forward ? 0x8000000B : 0x17;\n");
        fprintf(f, "    for (y = 0; y < 32; y++) {\n");
        fprintf(f, "       c = __builtin_parity(x & a);\n");
        fprintf(f, "       if (forward) x = (x<<1) | c;\n");
        fprintf(f, "       else         x = (x>>1) | (c<<31);\n");
        fprintf(f, "    }\n");
        fprintf(f, "    return x;\n");
        fprintf(f, "}\n");
        fprintf(f, "#define RandForw__(x) ((x) = LFSR__((x), 1))\n");
        fprintf(f, "#define RandBack__(x) ((x) = LFSR__((x), 0))\n");
    }
    if (parse->needsSqrt) {
        fprintf(f, "static uint32_t Sqrt__(uint32_t a) {\n");
        fprintf(f, "    uint32_t res = 0;\n");
        fprintf(f, "    uint32_t bit = 1<<30;\n");
        fprintf(f, "    while (bit > a) bit = bit>>2;\n");
        fprintf(f, "    while (bit != 0) {\n");
        fprintf(f, "        if (a >= res+bit) {\n");
        fprintf(f, "            a = a - (res+bit);\n");
        fprintf(f, "            res = (res>>1) + bit;\n");
        fprintf(f, "        } else res = res >> 1;\n");
        fprintf(f, "        bit = bit >> 2;\n");
        fprintf(f, "    }\n");
        fprintf(f, "    return res;\n");
        fprintf(f, "}\n"); 
    }
    if (parse->needsCogAccess) {
        // we need to execute code that looks like:
        //    mov r0, <addr>
        //    jmp <retaddr>
        // this is a bit tricky; we build it in r0,r1 and then jmp to it
        // with a jmpret r1,#0
        fprintf(f, "__asm__ volatile(\n");
        fprintf(f, "\"    .text\\n\"\n");
        fprintf(f, "\"    .balign 4\\n\"\n");
        fprintf(f, "\"__cog_xfer\\n\"\n");
        fprintf(f, "\"    fcache #(.Lend - .Lstart)\\n\"\n");
        fprintf(f, "\"    .compress off\\n\"\n");
        fprintf(f, "\".Lstart\\n\"\n");
        fprintf(f, "\"    mov pc, lr\\n\"\n");
        fprintf(f, "\"    mov lr, __LMM_RET\\n\"\n");
        fprintf(f, "\"    movd (.Linstr - .Lstart) + __LMM_FCACHE_START,r0\\n\"\n");
        fprintf(f, "\"    movs (.Linstr - .Lstart) + __LMM_FCACHE_START,r1\\n\"\n");
        fprintf(f, "\"    mov  r0,r2\\n\"\n");
        fprintf(f, "\".Linstr\\n\"\n");
        fprintf(f, "\"    mov  0-0,0-0\\n\"\n");
        fprintf(f, "\"    jmp  lr\\n\"\n");
        fprintf(f, "\".Lend\\n\"\n");
        fprintf(f, "\"    .compress default\\n\"\n");
        fprintf(f, ");\n");
        fprintf(f, "extern ");
        if (!gl_ccode) {
            fprintf(f, "\"C\" ");
        }
        fprintf(f, "int32_t _cog_xfer(int32_t dst, int32_t src, int32_t retval);\n");
        fprintf(f, "#define cogmem_get__(addr)      _cog_xfer(0, (addr), 0)\n");
        fprintf(f, "#define cogmem_put__(addr,data) _cog_xfer((addr), 0, (data))\n");
        fprintf(f, "\n");
    }
}

static void
PrintCppFile(FILE *f, ParserState *parse)
{
    /* things we always need */
    if (gl_header) {
        fprintf(f, "%s", gl_header);
    }
    if (parse->topcomment) {
        PrintComment(f, parse->topcomment);
    }
    if (parse->needsStdlib) {
        fprintf(f, "#include <stdlib.h>\n");
    }
    fprintf(f, "#include <propeller.h>\n");
    fprintf(f, "#include \"%s.h\"\n", parse->basename);
    fprintf(f, "\n");
    PrintMacros(f, parse);

    /* declare static functions and variables */
    if (gl_ccode && !gl_nospin) {
        int n;

        n = PrintPrivateFunctionDecls(f, parse);
        if (n > 0)
            fprintf(f, "\n");
    }
    /* print data block, if applicable */
    if (parse->datblock) {
        if (gl_gas_dat) {
            fprintf(f, "extern ");
            PrintDatArray(f, parse, ";\n", false);
            PrintDataBlockForGas(f, parse, 1);
        } else {
            if (gl_ccode) {
                fprintf(f, "static ");
                PrintDatArray(f, parse, " = {\n", false);
            } else {
                PrintDatArray(f, parse, " = {\n", true);
            }
            PrintDataBlock(f, parse, TEXT_OUTPUT);
            fprintf(f, "};\n");
        }
    }
    /* functions */
    PrintFunctionBodies(f, parse);

    /* any closing comments */
    if (parse->botcomment) {
        PrintComment(f, parse->botcomment);
    }

}

// output an assembly language statement
void
OutputAsmEquate(FILE *f, const char *str, unsigned int value)
{
    fprintf(f, "__asm__ volatile( \"    .global %s\\n\" );\n", str);
    fprintf(f, "__asm__ volatile( \"    %s = 0x%x\\n\" );\n", str, value);
}

// output _clkmode and _clkfreq settings
void
OutputClkFreq(FILE *f, ParserState *P)
{
    // look up in P->objsyms
    Symbol *clkmodesym = FindSymbol(&P->objsyms, "_clkmode");
    Symbol *sym;
    AST *ast;
    int32_t clkmode, clkfreq, xinfreq;
    int32_t multiplier = 1;
    uint8_t clkreg;
    
    if (!clkmodesym) {
        return;  // nothing to do
    }
    ast = (AST *)clkmodesym->val;
    if (clkmodesym->type != SYM_CONSTANT) {
        WARNING(ast, "_clkmode is not a constant");
        return;
    }
    clkmode = EvalConstExpr(ast);
    // now we need to figure out the frequency
    clkfreq = 0;
    sym = FindSymbol(&P->objsyms, "_clkfreq");
    if (sym) {
        if (sym->type == SYM_CONSTANT) {
            clkfreq = EvalConstExpr((AST*)sym->val);
        } else {
            WARNING((AST*)sym->val, "_clkfreq is not a constant");
        }
    }
    xinfreq = 0;
    sym = FindSymbol(&P->objsyms, "_xinfreq");
    if (sym) {
        if (sym->type == SYM_CONSTANT) {
            xinfreq = EvalConstExpr((AST*)sym->val);
        } else {
            WARNING((AST*)sym->val, "_xinfreq is not a constant");
        }
    }
    // calculate the multiplier
    clkreg = 0;
    if (clkmode & RCFAST) {
        // nothing to do here
    } else if (clkmode & RCSLOW) {
        clkreg |= 0x01;   // CLKSELx
    } else if (clkmode & (XINPUT)) {
        clkreg |= (1<<5); // OSCENA
        clkreg |= 0x02;   // CLKSELx
    } else {
        clkreg |= (1<<5); // OSCENA
        clkreg |= (1<<6); // PLLENA
        if (clkmode & XTAL1) {
            clkreg |= (1<<3);
        } else if (clkmode & XTAL2) {
            clkreg |= (2<<3);
        } else {
            clkreg |= (3<<3);
        }
        if (clkmode & PLL1X) {
            multiplier = 1;
            clkreg |= 0x3;  // CLKSELx
        } else if (clkmode & PLL2X) {
            multiplier = 2;
            clkreg |= 0x4;  // CLKSELx
        } else if (clkmode & PLL4X) {
            multiplier = 4;
            clkreg |= 0x5;  // CLKSELx
        } else if (clkmode & PLL8X) {
            multiplier = 8;
            clkreg |= 0x6;  // CLKSELx
        } else if (clkmode & PLL16X) {
            multiplier = 16;
            clkreg |= 0x7;  // CLKSELx
        }
    }
    
    // validate xinfreq and clkfreq
    if (xinfreq == 0) {
        if (clkfreq == 0) {
            ERROR(NULL, "Must set at least one of _XINFREQ or _CLKFREQ");
        }
    } else {
        int32_t calcfreq = xinfreq * multiplier;
        if (clkfreq != 0) {
            if (calcfreq != clkfreq) {
                ERROR(NULL, "Inconsistent values for _XINFREQ and _CLKFREQ");
            }
        }
        clkfreq = calcfreq;
    }
    
    // now output the clkfreq and clkmode settings
    OutputAsmEquate(f, "__clkfreqval", clkfreq);
    OutputAsmEquate(f, "__clkmodeval", clkreg);
}

void
OutputCppCode(const char *name, ParserState *P, int printMain)
{
    FILE *f = NULL;
    char *fname = NULL;
    ParserState *save;

    save = current;
    current = P;

    fname = malloc(strlen(name) + 8);

    /* print out the header file */
    sprintf(fname, "%s.h", name);
    f = fopen(fname, "w");
    if (!f) {
        perror(fname);
        free(fname);
        exit(1);
    }
    PrintDebugDirective(f, NULL);
    if (gl_ccode) {
        PrintCHeaderFile(f, P);
    } else {
        PrintCppHeaderFile(f, P);
    }
    fclose(f);

    if (gl_errors > 0) {
        free(fname);
        exit(1);
    }

    if (gl_ccode) {
        sprintf(fname, "%s.c", P->basename);
    } else {
        sprintf(fname, "%s.cpp", P->basename);
    }
    f = fopen(fname, "w");
    if (!f) {
        perror(fname);
        free(fname);
        exit(1);
    }
    PrintDebugDirective(f, NULL);
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

        //check for _clkmode and _clkfreq symbols
        OutputClkFreq(f, P);
        fprintf(f, "\n");

        if (gl_ccode) {
            fprintf(f, "%s MainObj__;\n\n", P->classname);
            fprintf(f, "int main() {\n");
            fprintf(f, "  return %s_%s(&MainObj__);\n", P->classname, defaultMethod->name);
            fprintf(f, "}\n");
        } else {
            fprintf(f, "%s MainObj__;\n\n", P->classname);
            fprintf(f, "int main() {\n");
            fprintf(f, "  return MainObj__.%s();\n", defaultMethod->name);
            fprintf(f, "}\n");
        }
    }
done:
    if (f) fclose(f);
    if (fname) free(fname);

    if (gl_errors > 0) {
        exit(1);
    }
    current = save;
}

static const char *lastfile = 0;
static int lastline = 0;

static void
EmitLineDirective(FILE *f, const char *name, int line)
{
    fprintf(f, "#line %d \"%s\"\n", line, name);
    lastfile = name;
    lastline = line;
}

void
PrintDebugDirective(FILE *f, AST *ast)
{
    int line;
    const char *name;

    if (!gl_debug) {
        return;
    }

    if (!ast) {
        // reset our knowledge
        name = NULL;
        line = 0;
        return;
    }
    line = ast->line;
    name = ast->fileName;

    if (lastfile != name) {
        EmitLineDirective(f, name, line);
        return;
    }
    if (lastline != line) {
        if (lastline < line) {
            int n = line - lastline;
            // if we're close, just synchronize by outputting blank lines
            if (n <= 5 && 0) {
                while (n-- > 0) {
                    fprintf(f, "\n");
                    lastline++;
                }
                return;
            }
        }
        EmitLineDirective(f, name, line);
    }
}

void
PrintNewline(FILE *f)
{
    fprintf(f, "\n");
    lastline++;
}
