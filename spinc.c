/*
 * Spin to C/C++ translator
 * Copyright 2011-2022 Total Spectrum Software Inc.
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
#include "mcpp/mcpp_lib.h"

//#define DEBUG_YACC

extern int spinyyparse(void);
extern int basicyyparse(void);
extern int cgramyyparse(void);

extern int spinyydebug;
extern int basicyydebug;
extern int cgramyydebug;

int gl_useFullPaths = 0;

static int
FindSymbolExact(SymbolTable *S, const char *name)
{
    Symbol *sym = FindSymbol(S, name);
    if (!sym)
        return 0;
    if (!strcmp(sym->our_name, name))
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
#include "sys/bytecode_rom.spin.h"
#include "sys/nucode_util.spin.h"
#include "sys/common_pasm.spin.h"
#include "sys/common.spin.h"
#include "sys/float.spin.h"
#include "sys/gcalloc.spin.h"
#include "sys/gc_pasm.spin.h"
#include "sys/gc_bytecode.spin.h"

void
InitGlobalModule(void)
{
    SymbolTable *table;
    Symbol *sym;
    int oldtmpnum;
    int saveyydebug;
    const char *syscode = "";
    
    current = systemModule = NewModule("_system_", LANG_SPIN_SPIN1);
    table = &systemModule->objsyms;

    sym = AddSymbol(table, "__clkfreq_var", SYM_VARIABLE, ast_type_long, NULL);
    sym->flags |= SYMF_GLOBAL;
    sym->offset = gl_p2 ? (P2_CONFIG_BASE+0x4) : 0;
    sym = AddSymbol(table, "__clkmode_var", SYM_VARIABLE, ast_type_byte, NULL);
    sym->flags |= SYMF_GLOBAL;
    sym->offset = gl_p2 ? (P2_CONFIG_BASE+0x8) : 4;

    sym = AddSymbol(table, "__sendptr", SYM_VARIABLE, ast_type_sendptr, NULL);
    sym->flags |= SYMF_GLOBAL;
    sym->offset = -1; // special flag for COG internal memory

    sym = AddSymbol(table, "__recvptr", SYM_VARIABLE, ast_type_recvptr, NULL);
    sym->flags |= SYMF_GLOBAL;
    sym->offset = -2; // special flag for COG internal memory

    if (gl_output != OUTPUT_BYTECODE || gl_interp_kind == INTERP_KIND_NUCODE) {
        if (gl_interp_kind == INTERP_KIND_NUCODE) {
            sym = AddSymbol(table, "__interp_dbase", SYM_VARIABLE, ast_type_ptr_long, NULL);
            sym->flags |= SYMF_GLOBAL;
            sym->offset = -0x1e0; // gives COG address
            sym = AddSymbol(table, "__interp_vbase", SYM_VARIABLE, ast_type_ptr_byte, NULL);
            sym->flags |= SYMF_GLOBAL;
            sym->offset = -0x1e2; // gives COG address
            sym = AddSymbol(table, "__interp_abortchain", SYM_VARIABLE, ast_type_ptr_long, NULL);
            sym->flags |= SYMF_GLOBAL;
            sym->offset = -0x1e4; // gives COG address
            
            sym = AddSymbol(table, "__interp_abortresult", SYM_VARIABLE, ast_type_ptr_long, NULL);
            sym->flags |= SYMF_GLOBAL;
            sym->offset = -0x1e5; // gives COG address
            
            sym = AddSymbol(table, "__interp_super", SYM_VARIABLE, ast_type_ptr_long, NULL);
            sym->flags |= SYMF_GLOBAL;
            sym->offset = -0x1e6; // gives COG address
            
            sym = AddSymbol(table, "__interp_temp1", SYM_VARIABLE, ast_type_generic, NULL);
            sym->flags |= SYMF_GLOBAL;
            sym->offset = -0x1ea; // special flag for COG internal memory
            sym = AddSymbol(table, "__interp_temp2", SYM_VARIABLE, ast_type_generic, NULL);
            sym->flags |= SYMF_GLOBAL;
            sym->offset = -0x1eb; // special flag for COG internal memory

            sym = AddSymbol(table, "__interp_sp", SYM_VARIABLE, ast_type_generic, NULL);
            sym->flags |= SYMF_GLOBAL;
            sym->offset = -0x1f8; // special flag for COG internal memory
        } else {
            sym = AddSymbol(table, "__lockreg", SYM_VARIABLE, ast_type_long, NULL);
            sym->flags |= SYMF_GLOBAL;
            sym->offset = -3; // special flag for COG internal memory
        }
    }

    if (gl_p2) {
        sym = AddSymbol(table, "_baudrate", SYM_VARIABLE, ast_type_byte, NULL);
        sym->flags |= SYMF_GLOBAL;
        sym->offset = P2_CONFIG_BASE+0xc;
    }
    
    /* compile inline assembly */
    if (gl_output == OUTPUT_ASM || gl_output == OUTPUT_COGSPIN || gl_output == OUTPUT_BYTECODE) {
        int old_normalize = gl_normalizeIdents;
        
        /* set up temporary variable processing */
        oldtmpnum = SetTempVariableBase(90000, 0);
        saveyydebug = spinyydebug;  // do not show parser debug output for system
        spinyydebug = 0;
        
        // add in processor specific code
        if (gl_output == OUTPUT_BYTECODE) {
            switch (gl_interp_kind) {
            case INTERP_KIND_P1ROM:
                syscode = (const char *)sys_bytecode_rom_spin;
                break;
            case INTERP_KIND_NUCODE:
                syscode = (const char *)sys_nucode_util_spin;
                break;
            default:
                ERROR(NULL, "No internal code for bytecode type\n");
                break;
            }
        } else {
            if (gl_p2) {
                syscode = (const char *)sys_p2_code_spin;
            } else {
                syscode = (const char *)sys_p1_code_spin;
            }
        }
        gl_normalizeIdents = 0;
        systemModule->Lptr = (LexStream *)calloc(sizeof(*systemModule->Lptr), 1);
        systemModule->Lptr->flags |= LEXSTREAM_FLAG_NOSRC;
        strToLex(systemModule->Lptr, syscode, "_system_", LANG_SPIN_SPIN1);
        spinyyparse();

        // add common PASM code
        if (gl_output != OUTPUT_BYTECODE) {
            strToLex(systemModule->Lptr, (const char *)sys_common_pasm_spin, "_common_pasm_", LANG_SPIN_SPIN1);
            spinyyparse();
        }
        strToLex(systemModule->Lptr, (const char *)sys_common_spin, "_common_", LANG_SPIN_SPIN1);
        spinyyparse();
        strToLex(systemModule->Lptr, (const char *)sys_float_spin, "_float_", LANG_SPIN_SPIN1);
        spinyyparse();
        strToLex(systemModule->Lptr, (const char *)sys_gcalloc_spin, "_gc_", LANG_SPIN_SPIN1);
        spinyyparse();
        if (gl_output == OUTPUT_BYTECODE) {
            strToLex(systemModule->Lptr, (const char *)sys_gc_bytecode_spin, "_platform_", LANG_SPIN_SPIN1);
            spinyyparse();
        } else {
            strToLex(systemModule->Lptr, (const char *)sys_gc_pasm_spin, "_platform_", LANG_SPIN_SPIN1);
            spinyyparse();
        }
        ProcessModule(systemModule);

        curfunc = NULL;
        /* restore temp variable base */
        (void)SetTempVariableBase(oldtmpnum, 89999);
        spinyydebug = saveyydebug;
        gl_normalizeIdents = old_normalize;
    }

}

int
CheckedTypeSize(AST *type)
{
    Module *P;
    if (IsClassType(type)) {
        P = GetClassPtr(type);
        if (P && P->pendingvarblock) {
            DeclareMemberVariables(P);
        }
    }
    return TypeSize(type);
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
    case '!':
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
            if (sym->kind != SYM_CONSTANT) {
                ERROR(identifier, "bad default type information");
            } else {
                typemask = EvalConstExpr((AST *)sym->val);
            }
            if (typemask & checkmask) {
                return ast_type_float;
            }
            return ast_type_long;
        }
    }
}

/* helper function for parsing pasm FILE directives */
AST *
GetFullFileName(AST *baseString)
{
    const char *basename;
    char *newname;
    AST *ret;

    if (baseString->kind == AST_EXPRLIST) {
        baseString = baseString->left;
    }
    if (baseString->kind != AST_STRING) {
        ERROR(baseString, "Expected string");
        basename = "error";
    } else {
        basename = baseString->d.string;
    }
    newname = find_file_on_path(&gl_pp, basename, NULL, current->fullname);
    if (!newname) {
        newname = strdup(basename);
    }
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
    datinfo = AddToList(NewAST(AST_ORGH, NULL, NULL), datinfo);
    P->datblock = AddToList(datinfo, P->datblock);

    // Add a definition for __basic_data_ptr
    MaybeDeclareMemberVar(P, varname, ast_type_string, 0, NORMAL_VAR);
    
    // now add in an initializer
    init = AstAssign(varname,
                     NewAST(AST_ADDROF, labelref, NULL));
    init = NewAST(AST_STMTLIST, init, NULL);
    P->body = AddToList(init, P->body);
}

/*
 * process a parsed module
 */
void
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
        P->body = NULL;
    }
    
    /* now declare all the symbols that weren't already declared */
    DeclareConstants(P, &P->conblock);
    ProcessConstants(P);
    DeclareMemberVariables(P);
    DeclareFunctions(P);

    /* recursively process closures */
    if (P->subclasses) {
        ProcessModule(P->subclasses);
    }
    DeclareModuleLabels(P);
    /* for all functions, do any language specific stuff */
    if (gl_errors < gl_max_errors) {
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
    if (IsBasicLang(language)) {
        basicyydebug = spinyydebug;
        basicyyparse();
    } else if (IsCLang(language)) {
        cgramyydebug = spinyydebug;
        cgramyyparse();
    } else {
        spinyyparse();
    }
    AstReportDone(&saveinfo);
}

static char *
getObjFileExtension(const char *fname)
{
    static char buf[1024];
    char *ext = 0;
    char *ptr;
    FILE *f = fopen(fname, "r");
    if (!f) {
        return NULL;
    }
    buf[0] = 0;
    ptr = fgets(buf, sizeof(buf), f);
    if (ptr) {
        if (!strncmp(ptr, "#line", 5)) {
            ptr = strrchr(ptr, '.');
            if (ptr) {
                ext = ptr;
                ptr = buf + strlen(buf) - 1;
                if (ptr[0] == '\n') {
                    ptr[0] = 0;
                    --ptr;
                }
                if (ptr[0] == '\"') {
                    ptr[0] = 0;
                }
            }
        }
    }
    fclose(f);
    return ext;
}

static Module *
doParseFile(const char *name, Module *P, int *is_dup)
{
    FILE *f = NULL;
    Module *save, *Q, *LastQ;
    char *fname = NULL;
    char *parseString = NULL;
    char *langptr;
    int language = LANG_SPIN_SPIN1;
    SymbolTable *saveCurrentTypes = NULL;
    int new_module = 0;

    // check language to process
    langptr = strrchr(name, '.');
    if (langptr) {
        if (!strcmp(langptr, ".o")) {
            // try to figure out language based on contents of file
            fname = find_file_on_path(&gl_pp, name, langptr, NULL);
            if (fname) {
                langptr = getObjFileExtension(fname);
            } else {
                langptr = NULL;
            }
            if (!langptr) {
                WARNING(NULL, "Unable to find file type for %s, assuming C", name);
                langptr = ".c";
            }
        }
        if (!strcmp(langptr, ".bas")
            || !strcmp(langptr, ".basic")
            || !strcmp(langptr, ".bi")
            )
        {
            language = LANG_BASIC_FBASIC;
        } else if (!strcmp(langptr, ".c")
                   || !strcmp(langptr, ".h")
                   || !strcmp(langptr, ".a")
            )
        {
            language = LANG_CFAMILY_C;
        } else if (!strcmp(langptr, ".cpp")
                   || !strcmp(langptr, ".cc")
                   || !strcmp(langptr, ".cxx")
                   || !strcmp(langptr, ".c++")
                   || !strcmp(langptr, ".hpp")
                   || !strcmp(langptr, ".hh")
            )
        {
            language = LANG_CFAMILY_CPP;
        }
        else if (!strcmp(langptr, ".spin2"))
        {
            language = LANG_SPIN_SPIN2;
            langptr = ".spin2";
        }
        else if (!strcmp(langptr, ".spin"))
        {
            language = LANG_SPIN_SPIN1;
            langptr = ".spin";
        }
        else if (gl_p2)
        {
            language = LANG_SPIN_SPIN2;
            langptr = ".spin2";
        }
        else
        {
            language = LANG_SPIN_SPIN1;
            langptr = ".spin";
        }
    } else {
        // no extension, see if we can figure one out
        // if currently compiling a Spin1 program assume Spin1
        // as the default
        if (current && current->mainLanguage == LANG_SPIN_SPIN1) {
            langptr = ".spin";
            language = LANG_SPIN_SPIN1;
        } else if (current && current->mainLanguage == LANG_SPIN_SPIN2) {
            langptr = ".spin2";
            language = LANG_SPIN_SPIN2;
        } else if (gl_p2) {
            langptr = ".spin2";
            language = LANG_SPIN_SPIN2;
        } else {
            langptr = ".spin";
            language = LANG_SPIN_SPIN1;
        }
    }
    if (current) {
        fname = find_file_on_path(&gl_pp, name, langptr, current->fullname);
        if (!fname) {
            if (!strcmp(langptr, ".spin2")) {
                fname = find_file_on_path(&gl_pp, name, ".spin", current->fullname);
                if (fname) {
                    language = LANG_SPIN_SPIN1;
                }
            } else if (!strcmp(langptr, ".spin")) {
                fname = find_file_on_path(&gl_pp, name, ".spin2", current->fullname);
                if (fname) {
                    language = LANG_SPIN_SPIN2;
                }
            }
        }
        if (fname) {
            fname = NormalizePath(fname);
        }
    } else if (!strcmp(langptr, ".a")) {
        fname = find_file_on_path(&gl_pp, name, langptr, NULL);
    }
    if (!fname) {
        fname = strdup(name);
    }
    if (gl_useFullPaths) {
        fname = (char *)MakeAbsolutePath(fname);
    }
    // check for file already included
    if (P) {
        Symbol *sym = FindSymbol(&P->objsyms, fname);
        if (sym) {
            if (sym->kind != SYM_FILE) {
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
        P->fromUsing = 1;
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
    currentTypes = (SymbolTable *)calloc(1, sizeof(*currentTypes));
    currentTypes->next = &P->objsyms;
    if (LangCaseInSensitive(language)) {
        currentTypes->flags = SYMTAB_FLAG_NOCASE;
    }
    AddSymbol(&P->objsyms, fname, SYM_FILE, (void *)0, NULL);
    
    if (gl_preprocess) {
        void *defineState;

#define MAX_MCPP_ARGC 255
        if (IsCLang(language)) {
            /* use mcpp */
            char *argv[MAX_MCPP_ARGC+1];
            int argc = 0;
            int r;
            const char *errString;
            argv[argc++] = "flexspin";
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
            if (language == LANG_SPIN_SPIN2) {
                static const char asmclk_def[] = "hubset\t##clkmode_ & !%11\013\twaitx\t##20_000_000/100\013\thubset\t##clkmode_";
                /* add predefined ASMCLK macro */
                pp_define(&gl_pp, "asmclk", asmclk_def);
                pp_define(&gl_pp, "Asmclk", asmclk_def);
                pp_define(&gl_pp, "AsmClk", asmclk_def);
                pp_define(&gl_pp, "ASMCLK", asmclk_def);
            }
            pp_run(&gl_pp);
            parseString = pp_finish(&gl_pp);
            pp_restore_define_state(&gl_pp, defineState);
        }
        strToLex(NULL, parseString, fname, language);
	doparse(language);
        free(parseString);
    } else {
        fileToLex(NULL, f, fname, language);
        doparse(language);
    }
    fclose(f);

    if (gl_errors >= gl_max_errors) {
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
    if (gl_errors >= gl_max_errors) {
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
        if (!pf->used_as_ptr) {
            if (pf->callSites == 0) {
                *oldptr = pf->next; // remove this function
            } else if (pf->body && pf->body->kind == AST_STRING) {
                *oldptr = pf->next; // also remove
            } else {
                oldptr = &pf->next;
            }
        } else {
            oldptr = &pf->next;
        }
    }
}

static void MarkStaticFunctionPointers(AST *list);

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
    Module *savecurrent = current;
    
    // mark everything unused
    for (P = allparse; P; P = P->next) {
        if (P->funcblock) {
            // make sure pending functions are declared
            DeclareFunctions(P);
        }
        for (pf = P->functions; pf; pf = pf->next) {
            if (P == allparse && !isBinary && pf->is_public && pf->body && pf->body->kind != AST_STRING) {
                pf->callSites = 1;
            } else {
                // top level public methods should be assumed called
                pf->callSites = 0;
            }
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
    // check for functions called via pointers
    for (P = allparse; P; P = P->next) {
        current = P;
        MarkStaticFunctionPointers(P->datblock);
        current = savecurrent;
    }
}

static void
MarkStaticFunctionPointers(AST *list)
{
    AST *sub;
    Symbol *sym;

    while (list) {
        switch (list->kind) {
        case AST_SIMPLEFUNCPTR:
            sub = list->left;
            sym = LookupAstSymbol(sub, NULL);
            if (sym) {
                Function *f;
                if (sym->kind != SYM_FUNCTION) {
                    ERROR(list, "%s is not a function", sym->user_name);
                    return;
                }
                f = (Function *)sym->val;
                f->used_as_ptr = 1;
                MarkUsed(f, "static func");
                MarkSystemFuncUsed("__call_methodptr");
            }
            break;
        default:
            MarkStaticFunctionPointers(list->left);
            break;
        }
        list = list->right;
    }
}

void
RemoveUnusedMethods(int isBinary)
{
    Module *P, *LastP;
    Function *pf;
    Module *saveCur;
    int keep;

    keep = (0 == (gl_optimize_flags & OPT_REMOVE_UNUSED_FUNCS));
    
    // mark everything unused
    // FIXME: there's an awful lot of duplication with
    // CheckUnusedMethods; can we simplify that?
    gl_features_used = 0;
    for (P = allparse; P; P = P->next) {
        for (pf = P->functions; pf; pf = pf->next) {
            pf->callSites = 0;
        }
    }
    
    if (isBinary && !keep) {
        MarkUsed(GetMainFunction(allparse), "__root__");
    } else {
        // mark stuff called via public functions
        for (P = allparse; P; P = P->next) {
            if (IsSystemModule(P)) continue;
            if (P->superclass) continue;
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
    // mark all functions called via pointers
    for (P = allparse; P; P = P->next) {
        for (pf = P->functions; pf; pf = pf->next) {
            if (pf->callSites == 0 && pf->used_as_ptr) {
                MarkUsed(pf, "__func pointer__");
            }
        }
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
        if (Q->funcblock) {
            DeclareFunctions(Q);
        }
    }
    for (Q = allparse; Q; Q = Q->next) {
        for (pf = Q->functions; pf; pf = pf->next) {
            if ((pf->callSites > 0) && pf->body) {
                if (pf->body->kind == AST_STRING) {
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
FixupFuncData(Module *P)
{
    Function *f;
    Function *savecur;

    if (gl_output == OUTPUT_CPP || gl_output == OUTPUT_C) {
        return;
    }
    for (f = P->functions; f; f = f->next) {
        if (f->extradecl) {
            AST *ast = f->extradecl;
            AST *decl;
            AST *table;
            AST *name;
            Symbol *sym;
            int tablelen;
            Label *label;

            savecur = curfunc;
            curfunc = f;
            while (ast) {
                if (ast->kind != AST_LISTHOLDER) {
                    ERROR(ast, "Internal error, expected list holder");
                    break;
                }
                decl = ast->left;
                if (decl->kind != AST_TEMPARRAYDECL) {
                    ERROR(ast, "Internal error, expected temp array decl");
                    break;
                }
                name = decl->left;  // this is the array def
                tablelen = EvalConstExpr(name->right);
                name = name->left;
                if (!IsIdentifier(name)) {
                    ERROR(ast, "Internal error, expected identifier");
                    break;
                }
                sym = FindSymbol(&P->objsyms, GetIdentifierName(name));
                if (!sym || sym->kind != SYM_TEMPVAR) {
                    ERROR(name, "Internal error, unable to find symbol");
                    break;
                }
                
                table = decl->right;
                if (table->kind != AST_EXPRLIST) {
                    ERROR(table, "Internal error, expected expression list");
                    break;
                }
                if (1 || !gl_p2) {
                    size_t newsize, padding;
                    AST *astpad;
                    newsize = (P->datsize + 3) & ~3; // round up to long boundary
                    padding = newsize - P->datsize;
                    if (padding) {
                        astpad = NewAST(AST_ARRAYDECL, AstInteger(0), AstInteger(padding));
                        astpad = NewAST(AST_EXPRLIST, astpad, NULL);
                        astpad = NewAST(AST_BYTELIST, astpad, NULL);
                        P->datblock = AddToList(P->datblock, astpad);
                    }
                    P->datsize = newsize;
                }
                label = (Label *)calloc(sizeof(*label), 1);
                sym->offset = label->hubval = P->datsize;
                label->type = ast_type_long;
                label->flags = LABEL_IN_HUB;
                sym->kind = SYM_LABEL;
                sym->val = (void *)label;
                table = NewAST(AST_LONGLIST, table, NULL);
                P->datblock = AddToList(P->datblock, table);
                P->datsize += tablelen * LONG_SIZE;
                ast = ast->right;
            }
            f->extradecl = NULL;
            curfunc = savecur;
        }
    }
}

/* insert a call to Q.funcname at start of function f */
static void
InsertInitCall(Function *f, Module *Q)
{
    ASTReportInfo saveinfo;
    Symbol *sym;
    const char *name = "__init__";
    Function *pf;
    AST *ident, *classtype, *expr;
    
    AstReportAs(f->body, &saveinfo); // reset error tracking
    
    // check that the function is static
    sym = LookupSymbolInTable(&Q->objsyms, name);
    if (!sym || !(sym->kind == SYM_FUNCTION)) {
        ERROR(f->body, "function %s not found in class %s", name, Q->classname);
        return;
    }
    
    pf = (Function *)sym->val;
    if (!pf->is_static) {
        WARNING(pf->body, "initialization function %s is not static", name);
    }
    ident = AstIdentifier(name);
    classtype = ClassType(Q);

    expr = NewAST(AST_CAST, classtype, AstInteger(0));
    expr = NewAST(AST_METHODREF, expr, ident);
    expr = NewAST(AST_FUNCCALL, expr, NULL);
    expr = NewAST(AST_STMTLIST, expr, f->body);
    f->body = expr;

    AstReportDone(&saveinfo);

}

/* P is top level module */
static void
FixupCode(Module *P, int isBinary)
{
    Module *Q, *LastQ, *subQ;
    int changes;
    Function *firstfunc;
    Function *pf;
    int sawfunc = 0;
    
    // insert sub-classes into the global list after their modules
    // and append the global module to the list
    if (allparse)
    {
        LastQ = NULL;
        for (Q = allparse; Q; Q = Q->next) {
            if (Q->functions || Q->subclasses) {
                sawfunc++;
            }
            LastQ = Q;
            subQ = Q->subclasses;
            if (subQ) {
                subQ->next = Q->next;
                Q->next = subQ;
            }
        }
        if (sawfunc) {
            LastQ->next = systemModule;
        }
    }

    /* perform generic high-level optimizations
       (including dead code removal) */
    if (gl_errors >= gl_max_errors) {
        return;
    }
    
    for (Q = allparse; Q; Q = Q->next) {
        if (Q->functions) {
            DoHighLevelOptimize(Q);
        }
        if (gl_errors) return;
    }
    
    do {
        CheckUnusedMethods(isBinary);
        changes = ResolveSymbols();
    } while (changes && gl_errors == 0);

    if (gl_errors >= gl_max_errors) {
        return;
    }
    RemoveUnusedMethods(isBinary);
    doTypeInference();
    
    for (Q = allparse; Q; Q = Q->next) {
        PerformCSE(Q);
    }

    // fix up any internal array references (e.g. for LOOKUP/LOOKDOWN)
    for (Q = allparse; Q; Q = Q->next) {
        FixupFuncData(Q);
    }
    
    // see if we need a heap for garbage collection
    {
        bool need_heap = false;
        for (pf = systemModule->functions; pf; pf = pf->next) {
            if (!strcasecmp(pf->name, "_gc_ptrs")) {
                need_heap = true;
                gl_features_used |= FEATURE_NEED_HEAP;
                break;
            }
        }
        if (need_heap) {
            Symbol *sym = FindSymbol(&P->objsyms, "HEAPSIZE");
            uint32_t heapsize = 0;
            AST *heapAst;

            if (!sym || sym->kind != SYM_CONSTANT) {
                Symbol *sym2;
                sym2 = FindSymbol(&P->objsyms, "heapsize");
                if (sym2) {
                    sym = sym2;
                }
            }
            if (!sym || sym->kind != SYM_CONSTANT) {
                Symbol *sym2;
                sym2 = FindSymbol(&P->objsyms, "_heapsize");
                if (sym2) {
                    sym = sym2;
                }
            }
            if (sym) {
                if (sym->kind != SYM_CONSTANT) {
                    WARNING(NULL, "heapsize is not a constant");
                } else {
                    heapsize = EvalConstExpr((AST *)sym->val);
                }
            } else if (gl_p2) {
                // if no symbol and we are on p2 change the default heap size
                heapsize = 6000; // 4096;
            }
            if (heapsize != 0) {
                // user changed heap size
                if (heapsize < 64) {
                    WARNING(NULL, "heapsize of %u is too small, using 64", heapsize);
                    heapsize = 64;
                }
                heapsize = (heapsize+3)/4; // convert to longs
                heapAst = AstInteger(heapsize);
                sym = FindSymbol(&systemModule->objsyms, "__real_heapsize__");
                if (!sym || sym->kind != SYM_CONSTANT) {
                    ERROR(NULL, "Internal error, could not find __REAL_HEAPSIZE__");
                } else {
                    // reset the size
                    sym->val = heapAst;
                }
            }
        }
    }

    /* insert calls to __init__ if needed */
    if (isBinary) {
        firstfunc = GetMainFunction(P);
    } else {
        firstfunc = NULL;
    }
    if (firstfunc) {
        for (Q = allparse; Q; Q = Q->next) {
            for (pf = Q->functions; pf; pf = pf->next) {
                if (pf->attributes & FUNC_ATTR_NEEDSINIT) {
                    InsertInitCall(firstfunc, Q);
                    break;
                }
            }
        }
    }
    /* sanity check */
    for (Q = allparse; Q; Q = Q->next) {
        if (Q->varsize_used_valid && Q->varsize_used != Q->varsize) {
            ERROR(NULL, "Internal error in module %s: varsize=%d but was %d when used in OBJ", Q->classname, Q->varsize, Q->varsize_used);
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
    if (P && gl_errors < gl_max_errors) {
        FixupCode(P, outputBin);
    }
    return P;
}

Function *
GetMainFunction(Module *P)
{
    const char *mainName = NULL;
    Function *pf;
    
    if (IsBasicLang(P->mainLanguage)) {
        mainName = "program";
    } else if (IsCLang(P->mainLanguage)) {
        if (gl_exit_status) {
            mainName = "_c_startup";
            P = systemModule;
        } else {
            mainName = "main";
        }
    }

    if (mainName && P->functions) {
        // look for function named "program" or "main"
        Function *f;
        for (f = P->functions; f; f = f->next) {
            if (!strcmp(f->name, mainName)) {
                return f;
            }
        }
        ERROR(NULL, "could not find function %s", mainName);
    }
    
    /* for Spin, return first public function */
    for (pf = P->functions; pf; pf = pf->next) {
        if (pf->is_public) {
            break;
        }
    }
    if (!pf && P->functions) {
        pf = P->functions;
        WARNING(NULL, "No PUB function found, using first PRI function `%s' instead", pf->name);
        pf->is_public = true;
    }
    return pf;
}
