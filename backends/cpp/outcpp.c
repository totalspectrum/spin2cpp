//
// C++ source code output for spin2cpp
//
// Copyright 2012-2018 Total Spectrum Software Inc.
// see the file COPYING for conditions of redistribution
//

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <ctype.h>
#include "spinc.h"
#include "outcpp.h"

/*
 * print out a declaration for the dat[] array
 * "tail" is appended to the declaration
 * if "classname" is TRUE, then add the class name as well
 */
static void
PrintDatArray(Flexbuf *f, Module *parse, const char *tail, bool classname)
{
    char *datname = parse->datname;

    flexbuf_printf(f, "char ");
    if (parse->datannotations) {
        PrintAnnotationList(f, parse->datannotations, ' ');
    }
    if (classname) {
        flexbuf_printf(f, "%s::", parse->classname);
    }
    flexbuf_printf(f, "%s[]%s", datname, tail);
}

/*
 * print out a comment string
 */
void
PrintCommentString(Flexbuf *f, const char *str, int indent)
{
    int numLines = 0;
    int lastNewLine = 0;
    const char *p;
    if (!str) return;
    for (p = str; *p; p++) {
        if (*p =='\n') {
            numLines++; lastNewLine = 1;
        } else {
            lastNewLine = 0;
        }
    }
    if (indent > 0)
        flexbuf_printf(f, "%*c", indent, ' ');
    if (numLines == 1 && lastNewLine == 1) {
        flexbuf_printf(f, "//");
        // C++ comments look much better with a space after the //; Spin
        // comments don't matter so much, so people leave them out.
        // optionally add the space
        if (!isspace(*str)) {
            flexbuf_printf(f, " ");
        }
        flexbuf_printf(f, "%s", str);  // note the string ended in a newline
        return;
    }
    flexbuf_printf(f, "/* ");
    while (str[0]) {
        if (str[0] == '*' && str[1] == '/') {
            flexbuf_putc('*', f);
            flexbuf_putc('%', f);
            str += 2;
        } else {
            flexbuf_putc(str[0], f);
            lastNewLine = (str[0] == '\n');
            if (lastNewLine && indent > 0) {
                flexbuf_printf(f, "%*c", indent, ' ');
            }
            str++;
        }
    }
    if (numLines > 0 && !lastNewLine) {
        flexbuf_printf(f, "\n");
        if (indent > 0)
            flexbuf_printf(f, "%*c", indent, ' ');
    }
    flexbuf_printf(f, " */\n");
}


/*
 * print out a list of comments
 */
void
PrintIndentedComment(Flexbuf *f, AST *ast, int indent)
{
    while (ast) {
        if (ast->kind == AST_SRCCOMMENT) {
            LineInfo *info = GetLineInfo(ast);
            if (info && info->linedata) {
                flexbuf_printf(f, "// %s", info->linedata);
            }
        } else {
            if (ast->kind != AST_COMMENT) {
                ERROR(ast, "Internal error: expected comment");
                return;
            }
            PrintCommentString(f, ast->d.string, indent);
        }
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
PrintConstant(Flexbuf *f, AST *ast)
{
    if (IsFloatConst(ast)) {
        PrintFloat(f, EvalConstExpr(ast), PRINTEXPR_DEFAULT);
        return;
    }
    PrintExpr(f, ast, PRINTEXPR_DEFAULT);
}

static void
PrintConstantDecl(Flexbuf *f, AST *ast)
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
    if (gl_output == OUTPUT_C || gl_gas_dat) {
        flexbuf_printf(f, "#define ");
        PrintSymbol(f, sym, PRINTEXPR_USECONST);
        flexbuf_printf(f, " (");
        PrintConstant(f, expr);
        flexbuf_printf(f, ")\n");
    } else {
        flexbuf_printf(f, "  static const int %s = ", ast->d.string);
        PrintConstant(f, expr);
        flexbuf_printf(f, ";\n");
    }
}

static int
PrintAllVarListsOfSize(Flexbuf *f, Module *parse, int siz, int flags)
{
    AST *ast;
    AST *upper;
    AST *comment;
    enum astkind kind;
    int n = 0;

    if (siz == 1)
        kind = AST_BYTELIST;
    else if (siz == 2)
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
            n += PrintVarList(f, siz, ast->left, flags);
        }
    }
    return n;
}

static void
PrintSubHeaders(Flexbuf *f, Module *parse)
{
    AST *ast, *sub;
    Module *objstate;
    int already_done;

    /* include any needed object headers */
    for (ast = parse->objblock; ast; ast = ast->right) {
        if (ast->kind != AST_OBJECT) {
            ERROR(ast, "Internal error: expected an OBJECT");
            break;
        }

        /* see if we've already printed this header */
        objstate = (Module *)ast->d.ptr;
        already_done = 0;
        for (sub = parse->objblock; sub && sub != ast; sub = sub->right) {
            if (sub->d.ptr == objstate) {
                already_done = 1;
                break;
            }
        }
        if (!already_done) {
            flexbuf_printf(f, "#include \"%s.h\"\n", objstate->basename);
        }
    }
    flexbuf_printf(f, "\n");

}

static void
PrintAllConstants(Flexbuf *f, Module *parse)
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
PrintObjectRefs(Flexbuf *f, Module *parse)
{
}
#endif

static void
PrintCHeaderFile(Flexbuf *f, Module *parse)
{
    int n;
    AST *ast;
    int flags = PRIVATE;
    CppModData *be = ModData(parse);

    /* things we always need */
    if (gl_header1 && gl_header2) {
        flexbuf_printf(f, "// %s", gl_header1);
        flexbuf_printf(f, "// %s", gl_header2);
    }
    flexbuf_printf(f, "#ifndef %s_Class_Defined__\n", parse->classname);
    flexbuf_printf(f, "#define %s_Class_Defined__\n\n", parse->classname);

    flexbuf_printf(f, "#include <stdint.h>\n");

    PrintSubHeaders(f, parse);
    PrintAllConstants(f, parse);

    if (be->needsTuple) {
        int n, i;
        for (n = 0; n < MAX_TUPLE; n++) {
            if (be->needsTuple & (1<<n)) {
                flexbuf_printf(f, "#ifndef Tuple%d__\n", n);
                flexbuf_printf(f, "  struct tuple%d__ {", n);
                for (i = 0; i < n; i++) {
                    flexbuf_printf(f, " int32_t v%d; ", i);
                }
                flexbuf_printf(f, "};\n");
                flexbuf_printf(f, "# define Tuple%d__ struct tuple%d__\n", n, n);
                flexbuf_printf(f, "#endif\n\n");
            }
        }
    }
    if (parse->volatileVariables)
        flags |= VOLATILE;

    /* print the structure definition */
    flexbuf_printf(f, "\ntypedef struct %s {\n", parse->classname);
    n = PrintAllVarListsOfSize(f, parse, 4, flags);
    n += PrintAllVarListsOfSize(f, parse, 2, flags);
    n += PrintAllVarListsOfSize(f, parse, 1, flags);

    /* object references */
    for (ast = parse->objblock; ast; ast = ast->right) {
        Module *P = (Module *)ast->d.ptr;
        AST *objdef = ast->left;
        if (objdef->kind == AST_IDENTIFIER) {
            flexbuf_printf(f, "  %s\t%s;\n", P->classname, objdef->d.string);
            n++;
        } else if (objdef->kind == AST_ARRAYDECL) {
            AST *arrname = objdef->left;
            AST *arrsize = objdef->right;
            if (arrname->kind == AST_IDENTIFIER) {
                flexbuf_printf(f, "  %s\t%s[%d];\n", P->classname,
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
        flexbuf_printf(f, "  char dummy__;\n");
    flexbuf_printf(f, "} %s;\n\n", parse->classname);

    /* now the public members */
    PrintPublicFunctionDecls(f, parse);

    /* now the private methods */
    /* PrintPrivateFunctionDecls(f, parse); */

    flexbuf_printf(f, "#endif\n");
}

static void
PrintCppHeaderFile(Flexbuf *f, Module *parse)
{
    AST *ast;
    int flags = PRIVATE;
    CppModData *be = ModData(parse);

    /* things we always need */
    if (gl_header1 && gl_header2) {
        flexbuf_printf(f, "// %s", gl_header1);
        flexbuf_printf(f, "// %s", gl_header2);
    }
    flexbuf_printf(f, "#ifndef %s_Class_Defined__\n", parse->classname);
    flexbuf_printf(f, "#define %s_Class_Defined__\n\n", parse->classname);

    flexbuf_printf(f, "#include <stdint.h>\n");

    /* include any needed object headers */
    PrintSubHeaders(f, parse);

    if (be->needsTuple) {
        int n, i;
        for (n = 0; n < MAX_TUPLE; n++) {
            if (be->needsTuple & (1<<n)) {
                flexbuf_printf(f, "#ifndef Tuple%d__\n", n);
                flexbuf_printf(f, "  struct tuple%d__ {", n);
                for (i = 0; i < n; i++) {
                    flexbuf_printf(f, " int32_t v%d; ", i);
                }
                flexbuf_printf(f, "};\n");
                flexbuf_printf(f, "# define Tuple%d__ struct tuple%d__\n", n, n);
                flexbuf_printf(f, "#endif\n\n");
            }
        }
    }

    /* print the constant declarations */
    flexbuf_printf(f, "class %s {\npublic:\n", parse->classname);

    PrintAllConstants(f, parse);

    /* object references */
    for (ast = parse->objblock; ast; ast = ast->right) {
        Module *P = (Module *)ast->d.ptr;
        AST *objdef = ast->left;
        if (objdef->kind == AST_IDENTIFIER) {
            flexbuf_printf(f, "  %s\t%s;\n", P->classname, objdef->d.string);
        } else if (objdef->kind == AST_ARRAYDECL) {
            AST *arrname = objdef->left;
            AST *arrsize = objdef->right;
            if (arrname->kind == AST_IDENTIFIER) {
                flexbuf_printf(f, "  %s\t%s[%d];\n", P->classname,
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
        flexbuf_printf(f, "  static ");
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
    flexbuf_printf(f, "private:\n");
    PrintAllVarListsOfSize(f, parse, 4, flags);
    PrintAllVarListsOfSize(f, parse, 2, flags);
    PrintAllVarListsOfSize(f, parse, 1, flags);

    /* now the private methods */
    PrintPrivateFunctionDecls(f, parse);
    flexbuf_printf(f, "};\n\n");
    flexbuf_printf(f, "#endif\n");
}

static void
PrintMacros(Flexbuf *f, Module *parse)
{
    CppModData *be = ModData(parse);
    bool needsIfdef = false;
    bool needsInline = false;
    if (gl_nospin)
        return;
    if (gl_output == OUTPUT_C) {
      needsIfdef = true;
      needsInline = true;
    }
    if (be->needsBitEncode
	  || be->needsMinMax
	  || be->needsRotate
	  || be->needsShr
	  || be->needsLookup
	  || be->needsLookdown)
      {
	needsInline = true;
      }

    if (needsIfdef) {
      flexbuf_printf(f, "#ifdef __GNUC__\n");
    }

    if (needsInline) flexbuf_printf(f, "#define INLINE__ static inline\n");
    if (be->needsYield) {
        // old way
        //flexbuf_printf(f, "#include <sys/thread.h>\n");
        //flexbuf_printf(f, "#define Yield__() (__napuntil(_CNT))\n");
        // new way -- not as thread friendly, but much faster
        flexbuf_printf(f, "#define Yield__() __asm__ volatile( \"\" ::: \"memory\" )\n");
    }
    if (be->needsHighmult) {
        flexbuf_printf(f, "#define Highmult__(X, Y) ( ( (X) * (int64_t)(Y) ) >> 32 )\n");
    }
    if (be->needsBitEncode) {
        flexbuf_printf(f, "#define BitEncode__(X) (32 - __builtin_clz(X))\n");
    }
    if (needsIfdef) 
    {
	flexbuf_printf(f, "#else\n");

	if (needsInline) flexbuf_printf(f, "#define INLINE__ static\n");
	if (be->needsYield) {
	  flexbuf_printf(f, "#define Yield__()\n");
	}
	if (gl_output == OUTPUT_C) {
	  flexbuf_printf(f, "#define waitcnt(n) _waitcnt(n)\n");
	  if (be->needsLockFuncs) {
              flexbuf_printf(f, "#define locknew() _locknew()\n");
              flexbuf_printf(f, "#define lockret(i) _lockret(i)\n");
              flexbuf_printf(f, "#define lockset(i) _lockset(i)\n");
              flexbuf_printf(f, "#define lockclr(i) _lockclr(i)\n");
	  }
	  flexbuf_printf(f, "#define coginit(id, code, par) _coginit((unsigned)(par)>>2, (unsigned)(code)>>2, id)\n");
	  flexbuf_printf(f, "#define cognew(code, par) coginit(0x8, (code), (par))\n");
	  flexbuf_printf(f, "#define cogstop(i) _cogstop(i)\n");
	  if (be->needsHighmult) {
              flexbuf_printf(f, "static int32_t Highmult__(int32_t a, int32_t b) {\n");
	      flexbuf_printf(f, "  int sign = (a^b)>>31;\n");
	      flexbuf_printf(f, "  uint32_t ua = a < 0 ? -a : a;\n");
	      flexbuf_printf(f, "  uint32_t ub = b < 0 ? -b : b;\n");
	      flexbuf_printf(f, "  uint32_t rhi = 0, rlo = 0;\n");
	      flexbuf_printf(f, "  int i;\n");
	      flexbuf_printf(f, "  for (i = 0; i < 32; i++) {\n");
	      flexbuf_printf(f, "    rhi = (rhi << 1) | (rlo >> 31);\n");
	      flexbuf_printf(f, "    rlo = rlo << 1;\n");
	      flexbuf_printf(f, "    if (ua >> 31) {rlo += ub; if (rlo < ub) rhi++;}\n");
	      flexbuf_printf(f, "    ua = ua<<1;\n");
	      flexbuf_printf(f, "  }\n");
	      flexbuf_printf(f, "  if (sign) { rhi = -rhi; rhi -= (rlo != 0); }\n");
	      flexbuf_printf(f, "  return rhi;\n");
	      flexbuf_printf(f, "}\n");
	  }
	  if (be->needsBitEncode) {
              flexbuf_printf(f, "INLINE__ int32_t BitEncode__(uint32_t a) {\n");
	      flexbuf_printf(f, "  int r=0;\n");
	      flexbuf_printf(f, "  while (a != 0) { a = a>>1; r++; }\n");
	      flexbuf_printf(f, "  return r;\n");
	      flexbuf_printf(f, "}\n");
	  }
	}
	
        flexbuf_printf(f, "#endif\n");
        flexbuf_printf(f, "\n");
    }
    if (be->needsMinMax) {
        flexbuf_printf(f, "INLINE__ int32_t Min__(int32_t a, int32_t b) { return a < b ? a : b; }\n"); 
        flexbuf_printf(f, "INLINE__ int32_t Max__(int32_t a, int32_t b) { return a > b ? a : b; }\n"); 
    }
    if (be->needsAbortdef) {
        if (gl_output == OUTPUT_CPP)
            flexbuf_printf(f, "extern \"C\" {\n");
        flexbuf_printf(f, "#include <setjmp.h>\n");
        if (gl_output == OUTPUT_CPP)
            flexbuf_printf(f, "}\n");
        flexbuf_printf(f, "typedef struct { jmp_buf jmp; int32_t val; } AbortHook__;\n");
        flexbuf_printf(f, "AbortHook__ *abortChain__ __attribute__((common));\n\n");
    }
    if (be->needsRotate) {
        flexbuf_printf(f, "INLINE__ int32_t Rotl__(uint32_t a, uint32_t b) { return (a<<b) | (a>>(32-b)); }\n"); 
        flexbuf_printf(f, "INLINE__ int32_t Rotr__(uint32_t a, uint32_t b) { return (a>>b) | (a<<(32-b)); }\n"); 
    }
    if (be->needsShr) {
        flexbuf_printf(f, "INLINE__ int32_t Shr__(uint32_t a, uint32_t b) { return (a>>b); }\n"); 
    }
    if (be->needsLookup) {
        flexbuf_printf(f, "INLINE__ int32_t Lookup__(int32_t x, int32_t b, int32_t a[], int32_t n) { int32_t i = (x)-(b); return ((unsigned)i >= n) ? 0 : (a)[i]; }\n");
        flexbuf_printf(f, "\n");
    }
    if (be->needsLookdown) {
        flexbuf_printf(f, "INLINE__ int32_t Lookdown__(int32_t x, int32_t b, int32_t a[], int32_t n) { int32_t i, r; r = 0; for (i = 0; i < n; i++) { if (a[i] == x) { r = i+b; break; } }; return r; }\n");
        flexbuf_printf(f, "\n");
    }
    if (be->needsRand) {
        flexbuf_printf(f, "static uint32_t LFSR__(uint32_t x, uint32_t forward) {\n");
        flexbuf_printf(f, "    uint32_t y, c, a;\n");
        flexbuf_printf(f, "    if (x < 1) x = 1;\n");
        flexbuf_printf(f, "    a = forward ? 0x8000000B : 0x17;\n");
        flexbuf_printf(f, "    for (y = 0; y < 32; y++) {\n");
        flexbuf_printf(f, "       c = __builtin_parity(x & a);\n");
        flexbuf_printf(f, "       if (forward) x = (x<<1) | c;\n");
        flexbuf_printf(f, "       else         x = (x>>1) | (c<<31);\n");
        flexbuf_printf(f, "    }\n");
        flexbuf_printf(f, "    return x;\n");
        flexbuf_printf(f, "}\n");
        flexbuf_printf(f, "#define RandForw__(x) ((x) = LFSR__((x), 1))\n");
        flexbuf_printf(f, "#define RandBack__(x) ((x) = LFSR__((x), 0))\n");
    }
    if (be->needsSqrt) {
        flexbuf_printf(f, "static uint32_t Sqrt__(uint32_t a) {\n");
        flexbuf_printf(f, "    uint32_t res = 0;\n");
        flexbuf_printf(f, "    uint32_t bit = 1<<30;\n");
        flexbuf_printf(f, "    while (bit > a) bit = bit>>2;\n");
        flexbuf_printf(f, "    while (bit != 0) {\n");
        flexbuf_printf(f, "        if (a >= res+bit) {\n");
        flexbuf_printf(f, "            a = a - (res+bit);\n");
        flexbuf_printf(f, "            res = (res>>1) + bit;\n");
        flexbuf_printf(f, "        } else res = res >> 1;\n");
        flexbuf_printf(f, "        bit = bit >> 2;\n");
        flexbuf_printf(f, "    }\n");
        flexbuf_printf(f, "    return res;\n");
        flexbuf_printf(f, "}\n"); 
    }
    if (be->needsCogAccess) {
        // we need to execute code that looks like:
        //    mov r0, <addr>
        //    jmp <retaddr>
        // this is a bit tricky; we build it in r0,r1 and then jmp to it
        // with a jmpret r1,#0
        flexbuf_printf(f, "__asm__(\n");
        flexbuf_printf(f, "\"    .text\\n\"\n");
        flexbuf_printf(f, "\"    .balign 4\\n\"\n");
        flexbuf_printf(f, "\"__cog_xfer\\n\"\n");
        flexbuf_printf(f, "\"    fcache #(.Lend - .Lstart)\\n\"\n");
        flexbuf_printf(f, "\"    .compress off\\n\"\n");
        flexbuf_printf(f, "\".Lstart\\n\"\n");
        flexbuf_printf(f, "\"    mov pc, lr\\n\"\n");
        flexbuf_printf(f, "\"    mov lr, __LMM_RET\\n\"\n");
        flexbuf_printf(f, "\"    movd (.Linstr - .Lstart) + __LMM_FCACHE_START,r0\\n\"\n");
        flexbuf_printf(f, "\"    movs (.Linstr - .Lstart) + __LMM_FCACHE_START,r1\\n\"\n");
        flexbuf_printf(f, "\"    mov  r0,r2\\n\"\n");
        flexbuf_printf(f, "\".Linstr\\n\"\n");
        flexbuf_printf(f, "\"    mov  0-0,0-0\\n\"\n");
        flexbuf_printf(f, "\"    jmp  lr\\n\"\n");
        flexbuf_printf(f, "\".Lend\\n\"\n");
        flexbuf_printf(f, "\"    .compress default\\n\"\n");
        flexbuf_printf(f, ");\n");
        flexbuf_printf(f, "extern ");
        if (gl_output == OUTPUT_CPP) {
            flexbuf_printf(f, "\"C\" ");
        }
        flexbuf_printf(f, "int32_t _cog_xfer(int32_t dst, int32_t src, int32_t retval);\n");
        flexbuf_printf(f, "#define cogmem_get__(addr)      _cog_xfer(0, (addr), 0)\n");
        flexbuf_printf(f, "#define cogmem_put__(addr,data) _cog_xfer((addr), 0, (data))\n");
        flexbuf_printf(f, "\n");
    }
    if (be->needsCoginit) {
        flexbuf_printf(f, "typedef void (*Cogfunc__)(void *a, void *b, void *c, void *d);\n");
        flexbuf_printf(f, "static void Cogstub__(void *argp) {\n");
        flexbuf_printf(f, "  void **arg = (void **)argp;\n");
        flexbuf_printf(f, "  Cogfunc__ func = (Cogfunc__)(arg[0]);\n");
        flexbuf_printf(f, "  func(arg[1], arg[2], arg[3], arg[4]);\n");
        flexbuf_printf(f, "}\n");
#if 1
        // this is more efficient, but relies on the clone_cog
        // function which is only available in newer libaries
        flexbuf_printf(f, "__asm__(\".global _cogstart\\n\"); // force clone_cog to link if it is present\n");
        if (gl_output == OUTPUT_CPP) {
            flexbuf_printf(f, "extern \"C\" ");
        } else {
            flexbuf_printf(f, "extern ");
        }
        flexbuf_printf(f, "void _clone_cog(void *tmp) __attribute__((weak));\n");
        if (gl_output == OUTPUT_CPP) {
            flexbuf_printf(f, "extern \"C\" ");
        } else {
            flexbuf_printf(f, "extern ");
        }
        flexbuf_printf(f, "long _load_start_kernel[] __attribute__((weak));\n");
        flexbuf_printf(f, "static int32_t Coginit__(int cogid, void *stackbase, size_t stacksize, void *func, int32_t arg1, int32_t arg2, int32_t arg3, int32_t arg4) {\n");
        flexbuf_printf(f, "    void *tmp = _load_start_kernel;\n");
        flexbuf_printf(f, "    unsigned int *sp = ((unsigned int *)stackbase) + stacksize/4;\n");
        flexbuf_printf(f, "    static int32_t cogargs__[5];\n");
        flexbuf_printf(f, "    int r;\n");
        flexbuf_printf(f, "    cogargs__[0] = (int32_t) func;\n");
        flexbuf_printf(f, "    cogargs__[1] = arg1;\n");
        flexbuf_printf(f, "    cogargs__[2] = arg2;\n");
        flexbuf_printf(f, "    cogargs__[3] = arg3;\n");
        flexbuf_printf(f, "    cogargs__[4] = arg4;\n");
        flexbuf_printf(f, "    if (_clone_cog) {\n");
        flexbuf_printf(f, "        tmp = __builtin_alloca(1984);\n");
        flexbuf_printf(f, "        _clone_cog(tmp);\n");
        flexbuf_printf(f, "    }\n");
        flexbuf_printf(f, "    *--sp = 0;\n");
        flexbuf_printf(f, "    *--sp = (unsigned int)cogargs__;\n");
        flexbuf_printf(f, "    *--sp = (unsigned int)Cogstub__;\n");
        flexbuf_printf(f, "    r = coginit(cogid, tmp, sp);\n");
        flexbuf_printf(f, "    return r;\n");
        flexbuf_printf(f, "}\n");
#else
        // NOTE: this one does not actually let you specify a cog :(
        flexbuf_printf(f, "static int32_t Coginit__(int cogid, void *stackbase, size_t stacksize, void *func, int32_t arg1, int32_t arg2, int32_t arg3, int32_t arg4) {\n");
        flexbuf_printf(f, "    static int32_t cogargs__[5];\n");
        flexbuf_printf(f, "    int r;\n");
        flexbuf_printf(f, "    cogargs__[0] = (int32_t) func;\n");
        flexbuf_printf(f, "    cogargs__[1] = arg1;\n");
        flexbuf_printf(f, "    cogargs__[2] = arg2;\n");
        flexbuf_printf(f, "    cogargs__[3] = arg3;\n");
        flexbuf_printf(f, "    cogargs__[4] = arg4;\n");
        flexbuf_printf(f, "    r = cogstart(Cogstub__, cogargs__, stackbase, stacksize);\n");
        flexbuf_printf(f, "    return r;\n");
        flexbuf_printf(f, "}\n");
#endif        
    }
}

#define BYTES_PER_LINE 16  /* must be at least 4 */
static int datacount;

static void
outputByteHex(Flexbuf *f, int c)
{
    if (datacount == 0) {
        flexbuf_printf(f, "  ");
    }
    flexbuf_printf(f, "0x%02x, ", c);
    datacount++;
    if (datacount == BYTES_PER_LINE) {
        flexbuf_printf(f, "\n");
        datacount = 0;
    }
}

static DataBlockOutFuncs cppOutputFuncs = {
    NULL,
    outputByteHex,
    NULL,
};

static void IfdefPropeller(Flexbuf *f)
{
    if (gl_cc) {
        flexbuf_printf(f, "#ifdef __propeller__\n");
    }
}
static void EndIfdefPropeller(Flexbuf *f)
{
    if (gl_cc) {
        flexbuf_printf(f, "#endif\n");
    }
}

static void
PrintCppFile(Flexbuf *f, Module *parse)
{
    /* things we always need */
    if (gl_header1 && gl_header2) {
        flexbuf_printf(f, "// %s", gl_header1);
        flexbuf_printf(f, "// %s", gl_header2);
    }
    if (parse->topcomment) {
        PrintComment(f, parse->topcomment);
    }
    if (ModData(parse)->needsStdlib) {
        flexbuf_printf(f, "#include <stdlib.h>\n");
    }
    IfdefPropeller(f);
    flexbuf_printf(f, "#include <propeller.h>\n");
    if (gl_gas_dat) {
        flexbuf_printf(f, "#undef clkset\n");
        flexbuf_printf(f, "#undef cogid\n");
        flexbuf_printf(f, "#undef cogstop\n");
        flexbuf_printf(f, "#undef locknew\n");
        flexbuf_printf(f, "#undef lockret\n");
        flexbuf_printf(f, "#undef lockclr\n");
        flexbuf_printf(f, "#undef lockset\n");
        flexbuf_printf(f, "#undef waitcnt\n");
        flexbuf_printf(f, "#undef waitpeq\n");
        flexbuf_printf(f, "#undef waitpne\n");
        flexbuf_printf(f, "#define _waitcnt(x) __builtin_propeller_waitcnt((x), 0)\n");
    }
    EndIfdefPropeller(f);
    flexbuf_printf(f, "#include \"%s.h\"\n", parse->basename);
    flexbuf_printf(f, "\n");
    PrintMacros(f, parse);

    /* declare static functions and variables */
    if (gl_output == OUTPUT_C && !gl_nospin) {
        int n;

        n = PrintPrivateFunctionDecls(f, parse);
        if (n > 0)
            flexbuf_printf(f, "\n");
    }
    /* print data block, if applicable */
    if (parse->datblock) {
        if (gl_gas_dat) {
            flexbuf_printf(f, "extern ");
            PrintDatArray(f, parse, " __asm__(\"..dat_start\");\n", false);
            PrintDataBlockForGas(f, parse, 1);
        } else {
            if (gl_output == OUTPUT_C) {
                flexbuf_printf(f, "static ");
                PrintDatArray(f, parse, " = {\n", false);
            } else {
                PrintDatArray(f, parse, " = {\n", true);
            }
            datacount = 0;
            PrintDataBlock(f, parse, &cppOutputFuncs, NULL);
            if (datacount != 0) {
                flexbuf_printf(f, "\n");
            }
            flexbuf_printf(f, "};\n");
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
OutputAsmEquate(Flexbuf *f, const char *str, unsigned int value)
{
    flexbuf_printf(f, "__asm__( \"    .global %s\\n\" );\n", str);
    flexbuf_printf(f, "__asm__( \"    %s = 0x%x\\n\" );\n", str, value);
}

void
OutputClkFreq(Flexbuf *f, Module *P)
{
    unsigned int clkfreq;
    unsigned int clkreg;

    if (GetClkFreq(P, &clkfreq, &clkreg)) {
        IfdefPropeller(f);
        // now output the clkfreq and clkmode settings
        OutputAsmEquate(f, "__clkfreqval", clkfreq);
        OutputAsmEquate(f, "__clkmodeval", clkreg);
        EndIfdefPropeller(f);
    }
}

static void
SetCppFlags(CppModData *bedata, AST *ast)
{
    if (!ast) return;
    switch(ast->kind) {
    case AST_YIELD:
        bedata->needsYield = 1;
        break;
    case AST_CATCH:
    case AST_ABORT:
        bedata->needsAbortdef = 1;
        bedata->needsStdlib = 1;
        break;
    case AST_SPRREF:
        bedata->needsCogAccess = 1;
        break;
    case AST_OPERATOR:
        {
            switch (ast->d.ival) {
            case K_HIGHMULT:
                bedata->needsHighmult = 1;
                break;
            case K_LIMITMIN:
            case K_LIMITMAX:
                bedata->needsMinMax = 1;
                break;
            case K_ROTL:
            case K_ROTR:
                bedata->needsRotate = 1;
                break;
            case K_SHR:
                bedata->needsShr = 1;
                break;
            case K_ABS:
                bedata->needsStdlib = 1;
                break;
            case K_SQRT:
                bedata->needsSqrt = 1;
                break;
            case K_ENCODE:
                bedata->needsBitEncode = 1;
                break;
            case '?':
                bedata->needsRand = 1;
                break;
            default:
                break;
            }
        }
        break;
    case AST_LOOKUP:
        bedata->needsLookup = 1;
        break;
    case AST_LOOKDOWN:
        bedata->needsLookdown = 1;
        break;
    case AST_RANGEREF:
        bedata->needsStdlib = 1; // range calc may invoke abs
        break;
    case AST_IDENTIFIER:
        {
            Symbol *sym = LookupSymbol(ast->d.string);
            if (sym && sym->type == SYM_BUILTIN) {
                Builtin *b = (Builtin *)sym->val;
                if (!strncmp(b->name, "lock", 4)) {
                    bedata->needsLockFuncs = 1;
                }
            }
        }
        break;
    case AST_RETURN:
        {
            AST *retval = ast->left;
            int n;
            if (!retval) {
                retval = curfunc->resultexpr;
            }
            if (retval) {
                if (retval->kind == AST_EXPRLIST) {
                    n = AstListLen(retval);
                    if (n > MAX_TUPLE) {
                        ERROR(retval, "Too many items in compound assignment");
                    }
                    bedata->needsTuple |= (1<<n);
                }
            }
        }
        break;
    case AST_ASSIGN:
        if (ast->left && ast->left->kind == AST_EXPRLIST) {
            AST *lhstyp = ExprType(ast->left);
            AST *rhstyp = ExprType(ast->right);
            int lhscount = (lhstyp && lhstyp->kind == AST_TUPLETYPE) ? lhstyp->d.ival : 1;
            int rhscount = (rhstyp && rhstyp->kind == AST_TUPLETYPE) ? rhstyp->d.ival : 1;
            if (lhscount != rhscount) {
                ERROR(ast, "Expected to assign %d items but found %d", lhscount, rhscount);
            } else {
                bedata->needsTuple |= (1<<lhscount);
            }
        }
        break;
    default:
        break;
    }
    SetCppFlags(bedata, ast->left);
    SetCppFlags(bedata, ast->right);
}

static void
CheckCppFlags(Module *P)
{
    Function *f;
    P->bedata = calloc(1, sizeof(CppModData));
    for (f = P->functions; f; f = f->next) {
        curfunc = f;
        SetCppFlags(ModData(P), f->body);
        if (f->cog_task) {
            ModData(P)->needsCoginit = 1;
        }
    }
}

void
OutputCppCode(const char *filename, Module *P, int printMain)
{
    FILE *f = NULL;
    char *fname = NULL;
    Module *save;
    Flexbuf fb;
    
    save = current;
    current = P;

    /* gather any specific info we need (like whether we will
       need to print certain macros) */
    CheckCppFlags(P);
    
    flexbuf_init(&fb, 0);
    PrintDebugDirective(&fb, NULL);
    if (gl_output == OUTPUT_C) {
        PrintCHeaderFile(&fb, P);
    } else {
        PrintCppHeaderFile(&fb, P);
    }
    
    if (gl_errors > 0) {
        exit(1);
    }

    /* print out the header file */
    fname = ReplaceExtension(filename, ".h");
    f = fopen(fname, "w");
    if (!f) {
        perror(fname);
        free(fname);
        exit(1);
    }
    fwrite(flexbuf_peek(&fb), flexbuf_curlen(&fb), 1, f);
    fclose(f);
    free(fname);
    flexbuf_delete(&fb);
    
    /* now do the C code */
    flexbuf_init(&fb, 0);
    PrintDebugDirective(&fb, NULL);

    PrintCppFile(&fb, P);
    if (printMain) {
        Function *defaultMethod = P->functions;
        while (defaultMethod && defaultMethod->name == NULL) {
            // skip over any dummy methods for annotations
            defaultMethod = defaultMethod->next;
        }
        if (defaultMethod == NULL) {
            ERROR(NULL, "unable to find default method for %s", P->classname);
            goto done;
        }
        if (defaultMethod->params) {
            ERROR(NULL, "default method of %s expects parameters", P->classname);
            goto done;
        }

        //check for _clkmode and _clkfreq symbols
        OutputClkFreq(&fb, P);
        flexbuf_printf(&fb, "\n");

        if (gl_output == OUTPUT_C) {
            flexbuf_printf(&fb, "%s MainObj__;\n\n", P->classname);
            flexbuf_printf(&fb, "int main() {\n");
            if (defaultMethod->is_static) {
                flexbuf_printf(&fb, "  %s_%s();\n", P->classname, defaultMethod->name);
            } else {
                flexbuf_printf(&fb, "  %s_%s(&MainObj__);\n", P->classname, defaultMethod->name);
            }
            flexbuf_printf(&fb, "  return 0;\n");
            flexbuf_printf(&fb, "}\n");
        } else {
            flexbuf_printf(&fb, "%s MainObj__;\n\n", P->classname);
            flexbuf_printf(&fb, "int main() {\n");
            flexbuf_printf(&fb, "  MainObj__.%s();\n", defaultMethod->name);
            flexbuf_printf(&fb, "  return 0;\n");
            flexbuf_printf(&fb, "}\n");
        }
    }

    if (gl_output == OUTPUT_C) {
        fname = ReplaceExtension(filename, ".c");
    } else {
        fname = ReplaceExtension(filename, ".cpp");
    }

    f = fopen(fname, "w");
    if (!f) {
        perror(fname);
        free(fname);
        exit(1);
    }
    fwrite(flexbuf_peek(&fb), flexbuf_curlen(&fb), 1, f);
    fclose(f);
    flexbuf_delete(&fb);
done:
    if (fname) free(fname);

    if (gl_errors > 0) {
        exit(1);
    }
    current = save;
}

static const char *lastfile = 0;
static int lastline = 0;

static void
EmitLineDirective(Flexbuf *f, const char *name, int line)
{
    if (name) {
        flexbuf_printf(f, "#line %d \"%s\"\n", line, name);
        lastfile = name;
        lastline = line;
    }
}

void
PrintDebugDirective(Flexbuf *f, AST *ast)
{
    int line;
    const char *name;
    LineInfo *info;

    if (!gl_debug) {
        return;
    }
    info = GetLineInfo(ast);
    
    if (!info) {
        // reset our knowledge
        name = NULL;
        line = 0;
        return;
    }
    line = info->lineno;
    name = info->fileName;

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
                    flexbuf_printf(f, "\n");
                    lastline++;
                }
                return;
            }
        }
        EmitLineDirective(f, name, line);
    }
}

void
PrintNewline(Flexbuf *f)
{
    flexbuf_printf(f, "\n");
    lastline++;
}
