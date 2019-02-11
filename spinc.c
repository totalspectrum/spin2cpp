/*
 * Spin to C/C++ translator
 * Copyright 2011-2019 Total Spectrum Software Inc.
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
#include "spin.tab.h"
#include "mcpp/mcpp_lib.h"

//#define DEBUG_YACC

extern int spinyyparse(void);
extern int basicyyparse(void);
extern int cgramyyparse(void);

extern int spinyydebug;
extern int basicyydebug;
extern int cgramyydebug;

// process a module after parsing it
static void ProcessModule(Module *P);

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
 * add global variable functions
 */

#include "sys/p1_code.spin.h"
#include "sys/p2_code.spin.h"
#include "sys/common.spin.h"
#include "sys/float.spin.h"
#include "sys/gcalloc.spin.h"

void
InitGlobalModule(void)
{
    SymbolTable *table;
    Symbol *sym;
    int oldtmpnum;
    const char *syscode;
    
    current = globalModule = NewModule("_system_", LANG_SPIN);
    table = &globalModule->objsyms;
    sym = AddSymbol(table, "_clkfreq", SYM_VARIABLE, ast_type_long);
    sym->flags |= SYMF_GLOBAL;
    sym->offset = gl_p2 ? (P2_CONFIG_BASE+0x4) : 0;
    sym = AddSymbol(table, "_clkmode", SYM_VARIABLE, ast_type_byte);
    sym->flags |= SYMF_GLOBAL;
    sym->offset = gl_p2 ? (P2_CONFIG_BASE+0x8) : 4;
    if (gl_p2) {
        sym = AddSymbol(table, "_baudrate", SYM_VARIABLE, ast_type_byte);
        sym->flags |= SYMF_GLOBAL;
        sym->offset = P2_CONFIG_BASE+0xc;
    }
    
    /* compile inline assembly */
    if (gl_output == OUTPUT_ASM || gl_output == OUTPUT_COGSPIN) {
        int old_normalize = gl_normalizeIdents;
        
        /* set up temporary variable processing */
        oldtmpnum = SetTempVariableBase(90000, 0);

        // add in processor specific code
        if (gl_p2) {
            syscode = (const char *)sys_p2_code_spin;
        } else {
            syscode = (const char *)sys_p1_code_spin;
        }
        gl_normalizeIdents = 0;
        globalModule->Lptr = malloc(sizeof(*globalModule->Lptr));
        strToLex(globalModule->Lptr, syscode, "_system_");
        spinyyparse();
        strToLex(globalModule->Lptr, (const char *)sys_common_spin, "_common_");
        spinyyparse();
        strToLex(globalModule->Lptr, (const char *)sys_float_spin, "_float_");
        spinyyparse();
        strToLex(globalModule->Lptr, (const char *)sys_gcalloc_spin, "_gc_");
        spinyyparse();
        
        ProcessModule(globalModule);

        curfunc = NULL;
        /* restore temp variable base */
        (void)SetTempVariableBase(oldtmpnum, 89999);
        gl_normalizeIdents = old_normalize;
    }

}

static int
DeclareMemberVariablesOfSize(Module *P, int basetypesize, int offset)
{
    AST *upper;
    AST *ast;
    AST *curtype;
    int curtypesize;
    int isUnion = P->isUnion;
    
    for (upper = P->pendingvarblock; upper; upper = upper->right) {
        AST *idlist;
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
            idlist = ast->left;
            break;
        case AST_WORDLIST:
            curtype = ast_type_word;
            curtypesize = 2;
            idlist = ast->left;
            break;
        case AST_LONGLIST:
	    curtype = NULL; // was ast_type_generic;
            curtypesize = 4;
            idlist = ast->left;
            break;
        case AST_DECLARE_VAR:
        case AST_DECLARE_VAR_WEAK:
            curtype = ast->left;
            idlist = ast->right;
            curtypesize = TypeSize(curtype);
            if (curtype->kind == AST_ASSIGN) {
                if (basetypesize == 4 || basetypesize == 0) {
                    ERROR(ast, "Member variables cannot have initial values");
                    return offset;
                }
            }
            break;
        case AST_COMMENT:
            /* skip */
            continue;
        default:
            ERROR(ast, "bad type  %d in variable list\n", ast->kind);
            return offset;
        }
        if (isUnion) {
            curtypesize = (curtypesize+3) & ~3;
            if (curtypesize > P->varsize) {
                P->varsize = curtypesize;
            }
        }
        if (basetypesize == 0) {
            // round offset up to necessary alignment
            if (!gl_p2) {
                if (curtypesize == 2) {
                    offset = (offset + 1) & ~1;
                } else if (curtypesize >= 4) {
                    offset = (offset + 3) & ~3;
                }
            }
            // declare all the variables
            offset = EnterVars(SYM_VARIABLE, &P->objsyms, curtype, idlist, offset, P->isUnion);
        } else if (basetypesize == curtypesize || (basetypesize == 4 && (curtypesize >= 4 || curtypesize == 0))) {
            offset = EnterVars(SYM_VARIABLE, &P->objsyms, curtype, idlist, offset, P->isUnion);
        }
    }
    return offset;
    
}

AST *
InferTypeFromName(AST *identifier)
{
    const char *name;
    const char *suffix;
    
    if (identifier->kind == AST_ASSIGN) {
        identifier = identifier->left;
    }
    if (identifier->kind == AST_ARRAYDECL) {
        identifier = identifier->left;
    }
    if (identifier->kind != AST_IDENTIFIER) {
        ERROR(identifier, "Internal error, expected identifier");
        return NULL;
    }
    suffix = name = identifier->d.string;
    if (!*name) {
        ERROR(identifier, "Internal error, empty identifier");
        return NULL;
    }
    while (suffix[1] != 0) {
        suffix++;
    }
    switch(suffix[0]) {
    case '$':
        return ast_type_string;
    case '%':
        return ast_type_long;
    case '#':
        return ast_type_float;
    default:
        {
            Symbol *sym = GetCurImplicitTypes();
            uint32_t typemask = 0;
            uint32_t firstlet;
            uint32_t checkmask;
            firstlet = toupper(name[0]);
            if (firstlet >= 'A' && firstlet <= 'Z') {
                checkmask = 1U << (firstlet - 'A');
            } else {
                return ast_type_long;
            }
            if (sym->type != SYM_CONSTANT) {
                ERROR(identifier, "bad default type information");
            } else {
                typemask = EvalConstExpr(sym->val);
            }
            if (typemask & checkmask) {
                return ast_type_float;
            }
            return ast_type_long;
        }
    }
}

//
// find any previous declaration of name
//
static AST *
FindDeclaration(AST *datlist, const char *name)
{
    AST *ident;
    AST *declare;
    
    while (datlist) {
        if (datlist->kind == AST_COMMENTEDNODE
            && datlist->left
            && datlist->left->kind == AST_DECLARE_VAR)
        {
            declare = datlist->left;
            ident = declare->right;
            if (ident->kind == AST_ASSIGN) {
                ident = ident->left;
            }
            if (ident->kind == AST_IDENTIFIER) {
                if (!strcmp(name, ident->d.string)) {
                    return declare;
                }
            } else {
                ERROR(ident, "Internal error, expected identifier while searching for %s", name);
                return NULL;
            }
        }
        datlist = datlist->right;
    }
    return NULL;
}

void
DeclareOneGlobalVar(Module *P, AST *ident, AST *type)
{
    AST *ast;
    AST *declare;
    AST *initializer = NULL;
    Symbol *olddef;
    SymbolTable *table = &P->objsyms;
    const char *name = NULL;
    const char *alias = NULL;
    int is_static = 0;
    int is_typedef = 0;
    
    if (!type) {
        type = InferTypeFromName(ident);
    }

    // this may be a typedef
    if (type->kind == AST_TYPEDEF) {
        type = type->left;
        is_typedef = 1;
    }
    if (type->kind == AST_STATIC) {
        // FIXME: this case probably shouldn't happen any more?? 
        type = type->left;
        is_static = 1;
    }
    if (type->kind == AST_FUNCTYPE) {
        // dummy declaration...
        return;
    }
    if (ident->kind == AST_ASSIGN) {
        if (is_typedef) {
            ERROR(ident, "typedef cannot have initializer");
        }
        initializer = ident->right;
        ident = ident->left;
    }
    if (ident->kind == AST_ARRAYDECL) {
        type = NewAST(AST_ARRAYTYPE, type, ident->right);
        type->d.ptr = ident->d.ptr;
        ident = ident->left;
    }
    if (ident->kind != AST_IDENTIFIER) {
        ERROR(ident, "Internal error, expected identifier");
        return;
    }
    name = ident->d.string;
    olddef = FindSymbol(table, name);
    if (olddef) {
        // is it an alias?
        if (olddef->type == SYM_ALIAS) {
            alias = name;
            name = olddef->val;
        }
    }

    if (is_typedef) {
        if (alias) {
            ERROR(ident, "Aliased typedef %s not allowed", alias);
        } else if (olddef) {
            ERROR(ident, "Redefining symbol %s", name);
        }
        AddSymbol(currentTypes, name, SYM_TYPEDEF, type);
        return;
    }
    if (olddef && !alias) {
        ERROR(ident, "Redefining symbol %s", name);
    }
    if (is_static) {
        if (!alias) {
            alias = NewTemporaryVariable("_static_");
            ident = AstIdentifier(alias);
            AddSymbol(table, name, SYM_ALIAS, (void *)alias);
        }
    }
    
    // if this is an array type with no size, there must be an
    // initializer
    if (type->kind == AST_ARRAYTYPE && !type->right) {
        if (!initializer) {
            ERROR(ident, "global array %s declared with no size and no initializer", name);
            type->right = AstInteger(1);
        } else {
            if (initializer->kind == AST_EXPRLIST) {
                type->right = AstInteger(AstListLen(initializer));
            } else {
                type->right = AstInteger(1);
            }
        }
    }
    // look through the globals to see if there's already a definition
    // if there is, and that definition has an initializer, we have a conflict
    declare = FindDeclaration(P->datblock, name);
    if (declare && declare->right) {
        if (declare->right->kind == AST_ASSIGN && initializer) {
            ERROR(initializer, "Variable %s is initialized twice",
                  alias ? alias : name);
        } else if (initializer) {
            declare->right = AstAssign(AstIdentifier(name), initializer);
        }
    } else {
        if (initializer) {
            ident = AstAssign(ident, initializer);
        }
        declare = NewAST(AST_DECLARE_VAR, type, ident);
        ast = NewAST(AST_COMMENTEDNODE, declare, NULL);
        P->datblock = AddToList(P->datblock, ast);
    }
    return;
}

void
DeclareOneMemberVar(Module *P, AST *ident, AST *type)
{
    if (!type) {
        type = InferTypeFromName(ident);
    }
    if (1) {
        AST *iddecl = NewAST(AST_LISTHOLDER, ident, NULL);
        AST *newdecl = NewAST(AST_DECLARE_VAR, type, iddecl);
        P->pendingvarblock = AddToList(P->pendingvarblock, NewAST(AST_LISTHOLDER, newdecl, NULL));
    }
}

void
MaybeDeclareMemberVar(Module *P, AST *identifier, AST *typ)
{
    AST *sub;
    const char *name;
    sub = identifier;
    if (sub && sub->kind == AST_ASSIGN) {
        sub = sub->left;
    }
    while (sub && sub->kind == AST_ARRAYDECL) {
        sub = sub->left;
    }
    if (!sub || sub->kind != AST_IDENTIFIER) {
        return;
    }
    name = GetIdentifierName(sub);
    Symbol *sym = FindSymbol(&P->objsyms, name);
    if (sym && sym->type == SYM_VARIABLE) {
        return;
    }
    if (!typ) {
        typ = InferTypeFromName(identifier);
    }
    if (!AstUses(P->pendingvarblock, identifier)) {
        AST *iddecl = NewAST(AST_LISTHOLDER, identifier, NULL);
        AST *newdecl = NewAST(AST_DECLARE_VAR, typ, iddecl);
        P->pendingvarblock = AddToList(P->pendingvarblock, NewAST(AST_LISTHOLDER, newdecl, NULL));
    }
}

void
DeclareMemberVariables(Module *P)
{
    int offset;

    if (P->isUnion) {
        offset = 0;
    } else {
        offset = P->varsize;
    }
    if (P->mainLanguage == LANG_SPIN) {
        // Spin always declares longs first, then words, then bytes
        // but other languages may have other preferences
        offset = DeclareMemberVariablesOfSize(P, 4, offset); // also declares >= 4
        offset = DeclareMemberVariablesOfSize(P, 2, offset);
        offset = DeclareMemberVariablesOfSize(P, 1, offset);
    } else {
        offset = DeclareMemberVariablesOfSize(P, 0, offset);
    }
    if (!P->isUnion) {
        // round up to next LONG boundary
        offset = (offset + 3) & ~3;
        P->varsize = offset;
    }
    P->finalvarblock = AddToList(P->finalvarblock, P->pendingvarblock);
    P->pendingvarblock = NULL;
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
 * initialize BASIC data block; copies the data into the datblock
 * section, and adds a RESTORE call to the start of the main program
 */
void
InitBasicData(Module *P)
{
    AST *labeldef = AstIdentifier("__basic_data");
    AST *labelref = AstIdentifier("__basic_data");
    AST *varname = AstIdentifier("__basic_data_ptr");
    AST *datinfo;
    AST *init;
    AST *zero;

    zero = NewAST(AST_BYTELIST, NewAST(AST_EXPRLIST, AstInteger(0), NULL),
                  NULL);
    datinfo = NewAST(AST_BYTELIST, P->bas_data, zero);
    datinfo = AddToList(labeldef, datinfo);
    datinfo = AddToList(NewAST(AST_LINEBREAK, NULL, NULL), datinfo);
    P->datblock = AddToList(datinfo, P->datblock);

    // Add a definition for __basic_data_ptr
    MaybeDeclareMemberVar(P, varname, ast_type_string);
    
    // now add in an initializer
    init = AstAssign(varname,
                     NewAST(AST_ADDROF, labelref, NULL));
    init = NewAST(AST_STMTLIST, init, NULL);
    P->body = AddToList(init, P->body);
}

/*
 * process a parsed module
 */
static void
ProcessModule(Module *P)
{
    Module *lastcurrent = current;

    current = P;
    P->botcomment = GetComments();

    if (P->body) {
        AST *funcdecl = NewAST(AST_FUNCDECL, AstIdentifier("program"), NULL);
        AST *funcvars = NewAST(AST_FUNCVARS, NULL, NULL);
        AST *funcdef = NewAST(AST_FUNCDEF, funcdecl, funcvars);

        /* if there is a basic DATA block, add an initializer */
        if (P->bas_data) {
            InitBasicData(P);
        }
        DeclareFunction(P, ast_type_void, 1, funcdef, P->body, NULL, NULL);
    }
    
    /* now declare all the symbols that weren't already declared */
    DeclareConstants(&P->conblock);
    DeclareMemberVariables(P); P->pendingvarblock = NULL;
    DeclareFunctions(P);

    /* recursively process closures */
    if (P->subclasses) {
        ProcessModule(P->subclasses);
    }
    DeclareLabels(P);
    /* for all functions, do any language specific stuff */
    if (gl_errors == 0) {
        Function *pf;
        for (pf = P->functions; pf; pf = pf->next) {
            ProcessOneFunc(pf);
        }
    }

    /* restore the default language */
    /* (we may have temporarily loaded functions written in another language) */
    P->curLanguage = P->mainLanguage;
    current = lastcurrent;
}

/*
 * parse a spin file
 * This is the main entry point for the compiler
 * "name" is the file name; if it has no .spin suffix
 * we'll try it with one
 * if "gl_depth" is >= 0 then print the file name
 */
int gl_depth = 0;

static void
doparse(int language)
{
    ASTReportInfo saveinfo;
    
    AstReportAs(NULL, &saveinfo); // reset error tracking
    if (language == LANG_BASIC) {
        basicyydebug = spinyydebug;
        basicyyparse();
    } else if (language == LANG_C) {
        cgramyydebug = spinyydebug;
        cgramyyparse();
    } else {
        spinyyparse();
    }
    AstReportDone(&saveinfo);
}

static Module *
doParseFile(const char *name, Module *P, int *is_dup)
{
    FILE *f = NULL;
    Module *save, *Q, *LastQ;
    char *fname = NULL;
    char *parseString = NULL;
    char *langptr;
    int language = LANG_SPIN;
    SymbolTable *saveCurrentTypes = NULL;
    int new_module = 0;

    // check language to process
    langptr = strrchr(name, '.');
    if (langptr) {
      if (!strcmp(langptr, ".bas")
          || !strcmp(langptr, ".basic")
          || !strcmp(langptr, ".bi")
          )
      {
	language = LANG_BASIC;
      } else if (!strcmp(langptr, ".c")
                 || !strcmp(langptr, ".h")
                 || !strcmp(langptr, ".cpp")
                 || !strcmp(langptr, ".cc")
                 || !strcmp(langptr, ".cxx")
          )
      {
	language = LANG_C;
      } else {
        langptr = ".spin";
      }
    } else {
      langptr = ".spin";
    }
    if (current) {
        fname = find_file_on_path(&gl_pp, name, langptr, current->fullname);
        if (fname) {
            fname = NormalizePath(fname);
        }
    }
    if (!fname) {
        fname = strdup(name);
    }
    // check for file already included
    if (P) {
        Symbol *sym = FindSymbol(&P->objsyms, fname);
        if (sym) {
            if (sym->type != SYM_FILE) {
                ERROR(NULL, "Expected FILE type for symbol %s", fname);
                return P;
            }
            if (is_dup) {
                *is_dup = 1;
            }
            return P;
        }
    }
    
    f = fopen(fname, "r");
    if (!f) {
        fprintf(stderr, "Unable to open file `%s': ", fname);
        perror("");
        free(fname);
        exit(1);
    }
    save = current;
    if (!P) {
        P = NewModule(fname, language);
        new_module = 1;
    }
    P->curLanguage = language;
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
    
    /* if we have already visited this file, skip it */
    /* also finds the last element in the list, so we can append to
       the list easily */
    if (new_module) {
        LastQ = NULL;
        for (Q = allparse; Q; Q = Q->next) {
            if (!strcmp(P->basename, Q->basename)) {
                free(fname);
                free(P);
                fclose(f);
                if (gl_printprogress) {
                    gl_depth--;
                }
                *is_dup = 1;
                return Q;
            }
            LastQ = Q;
        }
        if (LastQ)
            LastQ->next = P;
        else
            allparse = P;
    }
    current = P;
    saveCurrentTypes = currentTypes;
    currentTypes = calloc(1, sizeof(*currentTypes));
    currentTypes->next = &P->objsyms;

    AddSymbol(&P->objsyms, fname, SYM_FILE, (void *)0);
    
    if (gl_preprocess) {
        void *defineState;

#define MAX_MCPP_ARGC 255
        if (language == LANG_C) {
            /* use mcpp */
            char *argv[MAX_MCPP_ARGC+1];
            int argc = 0;
            int r;
            const char *errString;
            argv[argc++] = "fastspin";
            argc = pp_get_defines_as_args(&gl_pp, argc, argv, MAX_MCPP_ARGC);
            if (argc >= MAX_MCPP_ARGC-1) {
                ERROR(NULL, "ERROR: too many defines\n");
            }
            argv[argc++] = fname;
            argv[argc] = NULL;
            
            mcpp_use_mem_buffers(1);
            r = mcpp_lib_main(argc, argv);
            errString = mcpp_get_mem_buffer(ERR);
            parseString = mcpp_get_mem_buffer(OUT);
            if (r != 0) {
                if (errString) {
                    ERROR(NULL, "Preprocessor errors:\n%s", errString);
                } else {
                    ERROR(NULL, "Preprocessor failed!\n");
                }
                exit(1);
            } else if (errString && strlen(errString)) {
                WARNING(NULL, "Preprocessor warnings:\n%s", errString);
            }
        } else {
            SetPreprocessorLanguage(language);
            pp_push_file(&gl_pp, fname);
            defineState = pp_get_define_state(&gl_pp);
            pp_run(&gl_pp);
            parseString = pp_finish(&gl_pp);
            pp_restore_define_state(&gl_pp, defineState);
        }
        strToLex(NULL, parseString, fname);
	doparse(language);
        free(parseString);
    } else {
        fileToLex(NULL, f, fname);
        doparse(language);
    }
    fclose(f);

    if (gl_errors > 0) {
        free(fname);
        exit(1);
    }
    if (gl_printprogress) {
        gl_depth--;
    }

    if (new_module) {
        /* work to avoid conflicts with variables and constants */
        makeClassNameSafe(P);
        if (gl_gas_dat) {
            current->datname = (char *)malloc(strlen(P->classname) + 40);
            if (!current->datname) {
                fprintf(stderr, "Out of memory!\n");
                exit(2);
            }
            sprintf(current->datname, "_dat_%s_", P->classname);
        } else {
            current->datname = "dat";
        }
    }
    if (gl_errors > 0) {
        free(fname);
        exit(1);
    }

    current = save;
    currentTypes = saveCurrentTypes;
    return P;
}

static Module *
LoadFileIntoModule(const char *name, Module *P)
{
    int is_dup = 0;

    P = doParseFile(name, P, &is_dup);
    if (!is_dup) {
        ProcessModule(P);
    }
    return P;
}

Module *
ParseFile(const char *name)
{
    return LoadFileIntoModule(name, NULL);
}

//
// now that we've figured out what's reachable, remove
// uncalled methods from modules
//
static void
doPruneMethods(Module *P)
{
    Function *pf;
    Function **oldptr = &P->functions;
    for(;;) {
        pf = *oldptr;
        if (!pf) break;
        if (pf->callSites == 0 && pf->used_as_ptr == 0) {
            *oldptr = pf->next; // remove this function
        } else {
            oldptr = &pf->next;
        }
    }
}

//
// remove unused methods
// if "isBinary" is true we can eliminate any methods not called
// directly or indirectly from the main function
// Otherwise, we mark everything accessible from public functions
//
static void
CheckUnusedMethods(int isBinary)
{
    Module *P;
    Function *pf;

    // mark everything unused
    for (P = allparse; P; P = P->next) {
        for (pf = P->functions; pf; pf = pf->next) {
            pf->callSites = 0;
        }
    }
    
    if (isBinary) {
        MarkUsed(GetMainFunction(allparse), "__root__");
    } else {
        // mark stuff called via public functions
        for (P = allparse; P; P = P->next) {
            for (pf = P->functions; pf; pf = pf->next) {
                if (pf->callSites != 0) {
                    if (pf->is_public) {
                        MarkUsed(pf, "__public__");
                    } else if (pf->annotations) {
                        MarkUsed(pf, "__annotations__");
                    }
                }
            }
        }
    }
}

static void
MarkStaticFunctionPointers(AST *list)
{
    AST *sub;
    Symbol *sym;
    if (!list) {
        return;
    }
    switch (list->kind) {
    case AST_SIMPLEFUNCPTR:
        sub = list->left;
        sym = LookupAstSymbol(sub, NULL);
        if (sym) {
            Function *f;
            if (sym->type != SYM_FUNCTION) {
                ERROR(list, "%s is not a function", sym->name);
                return;
            }
            f = (Function *)sym->val;
            f->used_as_ptr = 1;
            MarkUsed(f, "static func");
        }
        return;
    default:
        MarkStaticFunctionPointers(list->left);
        MarkStaticFunctionPointers(list->right);
    }
}

void
RemoveUnusedMethods(int isBinary)
{
    Module *P, *LastP;
    Function *pf;
    Module *saveCur;
    
    // mark everything unused
    gl_features_used = 0;
    for (P = allparse; P; P = P->next) {
        for (pf = P->functions; pf; pf = pf->next) {
            pf->callSites = 0;
        }
    }
    
    if (isBinary) {
        MarkUsed(GetMainFunction(allparse), "__root__");
    } else {
        // mark stuff called via public functions
        for (P = allparse; P; P = P->next) {
            for (pf = P->functions; pf; pf = pf->next) {
                if (pf->callSites == 0) {
                    if (pf->is_public) {
                        MarkUsed(pf, "__public__");
                    } else if (pf->annotations) {
                        MarkUsed(pf, "__annotations__");
                    }
                }
            }
        }
    }
    // and check for function pointers in data
    for (P = allparse; P; P = P->next) {
        saveCur = current;
        current = P;
        MarkStaticFunctionPointers(P->datblock);
        current = saveCur;
        
    }
    // Now remove the ones that are never called
    for (P = allparse; P; P = P->next) {
        doPruneMethods(P);
    }

    // finally remove modules that have no defined functions and
    // no constant definitions or dat sections
    LastP = NULL;
    P = allparse;
    while (P) {
        if (!P->functions && !P->datblock && !P->conblock && !P->finalvarblock) {
            if (LastP) {
                LastP->next = P->next;
            } else {
                allparse = P->next;
            }
        }
        LastP = P;
        P = P->next;
    }
}

static int
ResolveSymbols()
{
    Module *Q, *savecurrent;
    Function *pf;
    int changes = 0;

    savecurrent = current;
    for (Q = allparse; Q; Q = Q->next) {
        for (pf = Q->functions; pf; pf = pf->next) {
            if ((pf->callSites > 0) && pf->body && pf->body->kind == AST_STRING) {
                const char *filename = pf->body->d.string;
                current = Q;
                LoadFileIntoModule(filename, pf->module);
                pf->callSites++;
                if (pf->body->kind == AST_STRING) {
                    ERROR(NULL, "No implementation for `%s' found in `%s'", pf->name, filename);
                } else {
                    changes = 1;
                }
            }
        }
    }
    current = savecurrent;
    return changes;
}

#define MAX_TYPE_PASSES 4

static void
doTypeInference(void)
{
    int tries = 0;
    int changes;
    Module *Q;
    Function *pf;
    // do type inference; we do that even for BASIC
    // because there are some things (like static-ness
    // of functions) that C wants to know about
    do {
        changes = 0;
        for (Q = allparse; Q; Q = Q->next) {
            changes += InferTypes(Q);
        }
    } while (changes != 0 && tries++ < MAX_TYPE_PASSES);
    // now update the types
    for (Q = allparse; Q; Q = Q->next) {
        for (pf = Q->functions; pf; pf = pf->next) {
            FixupParameterTypes(pf);
        }
    }
}

static void
FixupCode(Module *P, int isBinary)
{
    Module *Q, *LastQ, *subQ;
    int changes;
    
    // insert sub-classes into the global list after their modules
    // and append the global module to the list
    if (allparse)
    {
        LastQ = NULL;
        for (Q = allparse; Q; Q = Q->next) {
            LastQ = Q;
            subQ = Q->subclasses;
            if (subQ) {
                subQ->next = Q->next;
                Q->next = subQ;
            }
        }
        LastQ->next = globalModule;
    }

    do {
        CheckUnusedMethods(isBinary);
        changes = ResolveSymbols();
    } while (changes);
    RemoveUnusedMethods(isBinary);
    doTypeInference();
    
    for (Q = allparse; Q; Q = Q->next) {
        PerformCSE(Q);
    }
    // see if we need a heap for garbage collection
    {
        Function *pf;
        bool need_heap = false;
        for (pf = globalModule->functions; pf; pf = pf->next) {
            if (!strcasecmp(pf->name, "_gc_ptrs")) {
                need_heap = true;
                break;
            }
        }
        if (need_heap) {
            Symbol *sym = FindSymbol(&P->objsyms, "heapsize");
            uint32_t heapsize = 0;
            AST *heapAst;
            if (sym) {
                if (sym->type != SYM_CONSTANT) {
                    WARNING(NULL, "heapsize is not a constant");
                } else {
                    heapsize = EvalConstExpr((AST *)sym->val);
                }
            } else if (gl_p2) {
                // if no symbol and we are on p2 change the default heap size
                heapsize = 4096;
            }
            if (heapsize != 0) {
                // user changed heap size
                if (heapsize < 64) {
                    WARNING(NULL, "heapsize of %u is too small, using 64", heapsize);
                    heapsize = 64;
                }
                heapsize = (heapsize+3)/4; // convert to longs
                heapAst = AstInteger(heapsize);
                sym = FindSymbol(&globalModule->objsyms, "__real_heapsize__");
                if (!sym || sym->type != SYM_CONSTANT) {
                    ERROR(NULL, "Internal error, could not find __REAL_HEAPSIZE__");
                } else {
                    // reset the size
                    sym->val = heapAst;
                }
            }
        }
    }
}

Module *
ParseTopFiles(const char *argv[], int argc, int outputBin)
{
    const char *name;
    Module *P = NULL;
    int is_dup = 0; // not really used
    
    current = allparse = NULL;

    while (argc > 0) {
        name = *argv++;
        currentTypes = NULL;
        P = doParseFile(name, P, &is_dup);
        --argc;
    }
    ProcessModule(P);
    if (P && gl_errors == 0) {
        FixupCode(P, outputBin);
    }
    return P;
}

Function *
GetMainFunction(Module *P)
{
    const char *mainName = NULL;
    
    if (P->mainLanguage == LANG_BASIC) {
        mainName = "program";
    } else if (P->mainLanguage == LANG_C) {
        mainName = "main";
    }

    if (mainName) {
        // look for function named "program" or "main"
        Function *f;
        for (f = P->functions; f; f = f->next) {
            if (!strcmp(f->name, mainName)) {
                return f;
            }
        }
        ERROR(NULL, "could not find function %s", mainName);
    }
    return P->functions;
}
