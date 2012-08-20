//
// C++ source code output for spin2cpp
//
// Copyright 2012 Total Spectrum Software Inc.
// see the file COPYING for conditions of redistribution
//

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include "spinc.h"


/*
 * print out a header file
 */
static void
PrintConstantDecl(FILE *f, AST *ast)
{
    fprintf(f, "  static const int %s = ", ast->d.string);
    PrintInteger(f, EvalConstExpr(ast));
    fprintf(f, ";\n");
}

static void
PrintAllVarListsOfType(FILE *f, ParserState *parse, AST *type)
{
    AST *ast;
    enum astkind kind;

    if (type == ast_type_byte)
        kind = AST_BYTELIST;
    else if (type == ast_type_word)
        kind = AST_WORDLIST;
    else
        kind = AST_LONGLIST;
    
    for (ast = parse->varblock; ast; ast = ast->right) {
        if (ast->kind == kind) {
            PrintVarList(f, type, ast->left);
        }
    }
}

static void
PrintHeaderFile(FILE *f, ParserState *parse)
{
    AST *ast, *upper, *sub;
    int already_done;
    ParserState *objstate;

    /* things we always need */
    fprintf(f, "#ifndef %s_Class_Defined__\n", parse->classname);
    fprintf(f, "#define %s_Class_Defined__\n\n", parse->classname);

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
    if (gl_ccode) {
        /* nothing here?? */
    } else {
        fprintf(f, "class %s {\npublic:\n", parse->classname);
    }
    for (upper = parse->conblock; upper; upper = upper->right) {
        ast = upper->left;
        while (ast) {
            switch (ast->kind) {
            case AST_IDENTIFIER:
                PrintConstantDecl(f, ast);
                ast = ast->right;
                break;
            case AST_ASSIGN:
                PrintConstantDecl(f, ast->left);
                ast = NULL;
                break;
            default:
                /* do nothing */
                ast = ast->right;
                break;
            }
        }
    }
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
    if (parse->datblock) {
        fprintf(f, "  static uint8_t dat[];\n");
    }
    /* now the public members */
    PrintPublicFunctionDecls(f, parse);

    /* now the private members */
    /* Note that Spin sorts these, outputing first the 32 bit
       vars, then 16, finally 8
    */
    fprintf(f, "private:\n");
    PrintAllVarListsOfType(f, parse, ast_type_long);
    PrintAllVarListsOfType(f, parse, ast_type_word);
    PrintAllVarListsOfType(f, parse, ast_type_byte);

    /* now the private methods */
    PrintPrivateFunctionDecls(f, parse);
    if (!gl_ccode) {
        fprintf(f, "};\n\n");
    }
    fprintf(f, "#endif\n");
}

static void
PrintMacros(FILE *f, ParserState *parse)
{
    if (parse->needsYield) {
        fprintf(f, "#define Yield__() (__napuntil(_CNT))\n");
    }
    if (parse->needsMinMax) {
        fprintf(f, "extern inline int32_t Min__(int32_t a, int32_t b) { return a < b ? a : b; }\n"); 
        fprintf(f, "extern inline int32_t Max__(int32_t a, int32_t b) { return a > b ? a : b; }\n"); 
    }
    if (parse->needsAbortdef) {
        fprintf(f, "extern \"C\" {\n");
        fprintf(f, "#include <setjmp.h>\n");
        fprintf(f, "}\n");
        fprintf(f, "typedef struct { jmp_buf jmp; int32_t val; } AbortHook__;\n");
        fprintf(f, "AbortHook__ *abortChain__ __attribute__((common));\n\n");
    }
    if (parse->needsRotate) {
        fprintf(f, "extern inline int32_t Rotl__(uint32_t a, uint32_t b) { return (a<<b) | (a>>(32-b)); }\n"); 
        fprintf(f, "extern inline int32_t Rotr__(uint32_t a, uint32_t b) { return (a>>b) | (a<<(32-b)); }\n"); 
    }
    if (parse->needsShr) {
        fprintf(f, "extern inline int32_t Shr__(uint32_t a, uint32_t b) { return (a>>b); }\n"); 
    }
    if (parse->needsBetween) {
        fprintf(f, "extern inline int32_t Between__(int32_t x, int32_t a, int32_t b){ if (a <= b) return x >= a && x <= b; return x >= b && x <= a; }\n\n");
    }
    if (parse->needsLookup) {
        fprintf(f, "#define Lookup__(x, b, a, n) __extension__({ int32_t i = (x)-(b); ((unsigned)i >= n) ? 0 : (a)[i]; })\n");
        fprintf(f, "\n");
    }
    if (parse->needsLookdown) {
        fprintf(f, "#define Lookdown__(x, b, a, n) __extension__({ int32_t i, r; r = 0; for (i = 0; i < n; i++) { if (a[i] == x) { r = i+b; break; } }; r; })\n");
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
        if (gl_ccode) {
            fprintf(f, "static uint8_t %s_dat[] = {\n", parse->classname);
        } else {
            fprintf(f, "uint8_t %s::dat[] = {\n", parse->classname);
        }
        PrintDataBlock(f, parse, TEXT_OUTPUT);
        fprintf(f, "};\n");
    }
    /* functions */
    PrintFunctionBodies(f, parse);
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
    PrintHeaderFile(f, P);
    fclose(f);

    if (gl_errors > 0) {
        fprintf(stderr, "%d errors\n", gl_errors);
        free(fname);
        exit(1);
    }

    sprintf(fname, "%s.cpp", P->basename);
    f = fopen(fname, "w");
    if (!f) {
        perror(fname);
        free(fname);
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

        if (gl_ccode) {
            fprintf(f, "int main() {\n");
            fprintf(f, "  return %s_%s();\n", P->classname, defaultMethod->name);
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
        fprintf(stderr, "%d errors\n", gl_errors);
        exit(1);
    }
    current = save;
}
