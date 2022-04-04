/*
 * Spin to C/C++ translator
 * Copyright 2011-2021 Total Spectrum Software Inc.
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
#include <unistd.h>
#include "spinc.h"
#include "preprocess.h"
#include "version.h"
#include "cmdline.h"
#include "becommon.h"

static int TrivialSpinFunction(Module *P);

extern const char *gl_progname; /* defined in common.c */
const char *gl_cc = NULL;
const char *gl_intstring = "int32_t";

const char *gl_outname = NULL;

void InitializeSystem(CmdLineOptions *cmd, const char **argv)
{
    memset(cmd, 0, sizeof(*cmd));
    gl_src_charset = CHARSET_UTF8;
    gl_run_charset = CHARSET_UTF8;
    cmd->eepromSize = 32768;
    InitPreprocessor(argv);
    gl_max_errors = 1;
    gl_colorize_output = isatty(fileno(stderr));
    current_print_color = PRINT_NORMAL;
}

void
PrintFileSize(const char *fname)
{
    FILE *f = fopen(fname, "rb");
    unsigned len;

    if (f) {
        fseek(f, 0L, SEEK_END);
        len = ftell(f);
        fclose(f);
        printf("Program size is %u bytes\n", len);
    }
}

void
CompileAsmToBinary(const char *binname, const char *asmname)
{
    const char *listFile = gl_listing ? ReplaceExtension(binname, ".lst") : NULL;
    if (gl_errors > 0) {
        // no point in going all the way to compiling
        // the final assembly, it isn't valid anyway
        remove(binname);
        if (listFile) {
            remove(listFile);
        }
        exit(1);
    }
    gl_output = OUTPUT_DAT;
    gl_dat_offset = (gl_p2 ? 0 : DEFAULT_P1_DAT_OFFSET);
    gl_interp_kind = 0;
    
    Module *Q = ParseTopFiles(&asmname, 1, 1);
    if (gl_errors == 0) {
        if (listFile) {
            OutputLstFile(listFile, Q);
        }
        OutputDatFile(binname, Q, 1);
    }
}

int ProcessCommandLine(CmdLineOptions *cmd)
{
    Module *P;
    const char *listFile = NULL;
    
    if (gl_output == OUTPUT_COGSPIN) {
        gl_optimize_flags &= ~OPT_REMOVE_UNUSED_FUNCS;
    }
    if (gl_relocatable) {
        if (!gl_p2) {
            fprintf(stderr, "ERROR: relocatable output only supported for P2\n");
            exit(1);
        }
        gl_no_coginit = 1;
        if (gl_hub_base != 0) {
            fprintf(stderr, "Warning: --relocatable overrides -H\n");
            gl_hub_base = 0;
        }
    }
    /* add some predefined symbols */
    
    if (gl_p2) {
        pp_define(&gl_pp, "__propeller__", "2");
    } else {
        pp_define(&gl_pp, "__propeller__", "1");
    }
    pp_define(&gl_pp, "__FASTSPIN__", str_(VERSION_MAJOR));
    pp_define(&gl_pp, "__FLEXSPIN__", str_(VERSION_MAJOR));
    pp_define(&gl_pp, "__FLEXBASIC__", str_(VERSION_MAJOR));
    pp_define(&gl_pp, "__FLEXC__", str_(VERSION_MAJOR));
    pp_define(&gl_pp, "__SPINCVT__", str_(VERSION_MAJOR));
    {
        // pp_define does not do a strdup() on the data, so we need
        // to do that
        char quote_data[256];
        time_t now = time(NULL);
        struct tm *tm = localtime(&now);
        
        sprintf(quote_data, "\"%s\"", version_string);
        pp_define(&gl_pp, "__VERSION__", strdup(quote_data));

        strftime(quote_data, sizeof(quote_data), "\"%b %d %Y\"",  tm);
        pp_define(&gl_pp, "__DATE__", strdup(quote_data));

        strftime(quote_data, sizeof(quote_data), "\"%H:%M:%S\"",  tm);
        pp_define(&gl_pp, "__TIME__", strdup(quote_data));
    }
    if (gl_exit_status) {
        pp_define(&gl_pp, "__EXIT_STATUS__", "1");
    }
    if (gl_output == OUTPUT_ASM || gl_output == OUTPUT_COGSPIN) {
        pp_define(&gl_pp, "__SPIN2PASM__", "1");
        pp_define(&gl_pp, "__OUTPUT_ASM__", "1");
    } else if (gl_output == OUTPUT_CPP || gl_output == OUTPUT_C) {
        pp_define(&gl_pp, "__SPIN2CPP__", "1");
        if (gl_output == OUTPUT_CPP) {
            pp_define(&gl_pp, "__cplusplus", "1");
            pp_define(&gl_pp, "__OUTPUT_CPP__", "1");
        } else {
            pp_define(&gl_pp, "__OUTPUT_C__", "1");
        }
    } else if (gl_output == OUTPUT_BYTECODE) {
        pp_define(&gl_pp, "__OUTPUT_BYTECODE__", "1");
        if (gl_interp_kind == INTERP_KIND_P1ROM) {
            pp_define(&gl_pp, "__OUTPUT_BYTECODE_P1ROM__", "1");
        } else if (gl_interp_kind == INTERP_KIND_P2SPIN) {
            pp_define(&gl_pp, "__OUTPUT_BYTECODE_P2SPIN__", "1");
        } else if (gl_interp_kind == INTERP_KIND_NUCODE) {
            pp_define(&gl_pp, "__OUTPUT_BYTECODE_NUCODE__", "1");
        }
    }
    pp_define(&gl_pp, "__ILP32__", "1");
    if (gl_p2) {
        pp_define(&gl_pp, "__P2__", "1");
        pp_define(&gl_pp, "__propeller2__", "1");
    }
    if (gl_fixedreal) {
        pp_define(&gl_pp, "__fixedreal__", "1");
    }
    // Note: replicating logic from GuessFcacheSize here, kinda terrible
    if (gl_fcache_size > 0 || (gl_fcache_size == -1 && (!gl_p2 || (gl_optimize_flags&OPT_AUTO_FCACHE)))) {
        pp_define(&gl_pp,"__HAVE_FCACHE__","1");
    }
    /* if -Oremove-features is not on, then assume certain features are present */
    if ( 0 == (gl_optimize_flags & OPT_REMOVE_FEATURES) ) {
        unsigned i, flag;
        for (i = 0; i < 32; i++) {
            flag = 1U << i;
            if ( (FEATURE_DEFAULTS_NOOPT) & flag ) {
                ActivateFeature(flag);
            }
        }
    }
    /* set up the binary offset */
    gl_dat_offset = -1; // by default offset is unknown
    if ( (gl_output == OUTPUT_DAT||gl_output == OUTPUT_ASM) && cmd->outputBin) {
        // a 24 byte spin header is prepended to binary output of dat
        // (but not in p2 mode)
        gl_dat_offset = gl_p2 ? 0 : DEFAULT_P1_DAT_OFFSET;
    } else if (gl_output == OUTPUT_DAT && gl_gas_dat) {
        // GAS output for dat uses symbols, so @@@ is OK there
        gl_dat_offset = 0;
    }

    // compression
    if (gl_compress) {
        gl_lmm_kind = LMM_KIND_COMPRESS;
        gl_fcache_size = 0;
        gl_optimize_flags |= OPT_REMOVE_HUB_BSS;
        if (gl_p2) {
            fprintf(stderr, "-z is not supported on P2 yet");
            exit(2);
        }
    }
    // listing file
    if (gl_listing && !(gl_output == OUTPUT_DAT)) {
        gl_srccomments = 1;
    }
    
    /* initialize the parser; we do that after command line processing
       so that command line options can influence it */
    Init();

    /* now actually parse the file */
    if (!cmd->quiet) {
        gl_printprogress = 1;
    }

    P = ParseTopFiles(cmd->file_argv, cmd->file_argc, cmd->outputBin);

    if (cmd->outputFiles) {
        Module *Q;
        for (Q = allparse; Q; Q = Q->next) {
            printf("%s\n", Q->fullname);
        }
        return 0;
    }
    
    if (P) {
        int compile_original = 0;
        
        if (gl_errors >= gl_max_errors) {
            return 1;
        }
        /* set up output file names */
        if (gl_listing) {
            listFile = ReplaceExtension(P->fullname, ".lst");
        }
    
        if (gl_output == OUTPUT_OBJ) {
            const char *binname = gl_outname;
            if (!binname) {
                binname = ReplaceExtension(P->fullname, ".o");
            }
            OutputObjFile(binname, P);
        } else if (cmd->outputDat) {
            cmd->outname = gl_outname;
            if (gl_gas_dat) {
	            if (!cmd->outname) {
                    cmd->outname = ReplaceExtension(P->fullname, ".S");
                }
                OutputGasFile(cmd->outname, P);
            } else {
	            if (!cmd->outname) {
                    if (cmd->outputBin) {
                        if (cmd->useEeprom) {
                            cmd->outname = ReplaceExtension(P->fullname, ".eeprom");
                        } else {
                            cmd->outname = ReplaceExtension(P->fullname, ".binary");
                        }
                    } else {
                        cmd->outname = ReplaceExtension(P->fullname, ".dat");
                    }
                }
                if (cmd->bstcMode && !listFile) {
                    cmd->outname = ReplaceExtension(cmd->outname, ".binary");
                }
                if (listFile) {
                    OutputLstFile(listFile, P);
                }
                OutputDatFile(cmd->outname, P, cmd->outputBin);
                if (cmd->outputBin) {
                    DoPropellerPostprocess(cmd->outname, cmd->useEeprom ? cmd->eepromSize : 0);
                }
            }
        } else if (cmd->outputAsm || (cmd->outputBytecode && gl_interp_kind == INTERP_KIND_NUCODE) ) {
            const char *binname = NULL;
            const char *asmname = NULL;
            if (cmd->compile) {
                binname = gl_outname;
                if (binname) {
                    asmname = ReplaceExtension(binname, gl_p2 ? ".p2asm" : ".pasm");
                } else {
                    if (cmd->useEeprom) {
                        binname = ReplaceExtension(P->fullname, ".eeprom");
                    } else {
                        binname = ReplaceExtension(P->fullname, ".binary");
                    }
                }
            } else {
                asmname = gl_outname;
            }
            if (!asmname) {
                if (gl_output == OUTPUT_COGSPIN) {
                    asmname = ReplaceExtension(P->fullname, ".cog.spin");
                } else {
                    asmname = ReplaceExtension(P->fullname, gl_p2 ? ".p2asm" : ".pasm");
                }
            }
            if (TrivialSpinFunction(P)) {
                // we can just assemble the .spin file directly
                asmname = strdup(P->fullname);
                compile_original = 1;
            } else if (gl_interp_kind == INTERP_KIND_NUCODE) {
                if (!gl_p2) {
                    ERROR(NULL, "Nucode only supported on P2");
                }
                OutputNuCode(asmname, P);
            } else {
                OutputAsmCode(asmname, P, cmd->outputMain);
            }
            if (cmd->compile)  {
                gl_caseSensitive = !compile_original;
                gl_warn_flags &= ~WARN_ASM_USAGE; // already issued warnings
                CompileAsmToBinary(binname, asmname);
                DoPropellerPostprocess(binname, cmd->useEeprom ? cmd->eepromSize : 0);
                if (!cmd->quiet) {
                    printf("Done.\n");
                    if (gl_errors == 0) {
                        PrintFileSize(binname);
                    }
                }
            }
        } else if (cmd->outputBytecode) {
            if (!cmd->outname) {
                if (cmd->useEeprom) {
                    cmd->outname = ReplaceExtension(P->fullname, ".eeprom");
                } else {
                    cmd->outname = ReplaceExtension(P->fullname, ".binary");
                }
            }

            if (gl_interp_kind == INTERP_KIND_NUCODE) {
                ERROR(NULL, "How did we get here?");
            } else {
                OutputByteCode(cmd->outname,P);
                DoPropellerPostprocess(cmd->outname,cmd->useEeprom ? cmd->eepromSize : 0);
            }
        } else {
            fprintf(stderr, "This front end cannot convert to C\n");
        }
    } else {
        fprintf(stderr, "parse error\n");
        return 1;
    }

    return 0;
}

const char * GetOptionString(char *buf, size_t n, const char *opts)
{
    int i = 0;
    int c;
    int balance = 0;

    if (n <= 1) {
        return NULL;
    }
    --n;
    for(;;) {
        c = *opts;
        if (c == 0) {
            *buf = 0;
            return opts;
        }
        if (c == ')' && balance == 0) {
            *buf = 0;
            return NULL;
        }
        opts++;
        if (c == ',') {
            *buf = 0;
            return opts;
        }
        if (i < n) {
            *buf++ = c;
            i++;
        }
        if (c == '(') {
            balance++;
        } else if (c == ')') {
            balance--;
        }
    }
}

static struct optflag_table {
    const char *name;
    int bits;
} optflag[] = {
    { "remove-unused", OPT_REMOVE_UNUSED_FUNCS },
    { "remove-features", OPT_REMOVE_FEATURES },
    { "remove-dead", OPT_DEADCODE },
    { "inline-small", OPT_INLINE_SMALLFUNCS },
    { "regs", OPT_BASIC_REGS },
    { "branch-convert", OPT_BRANCHES },
    { "const", OPT_CONST_PROPAGATE },
    { "peephole", OPT_PEEPHOLE },
    { "tail-calls", OPT_TAIL_CALLS },
    { "loop-basic", OPT_LOOP_BASIC },
    { "loop-reduce", OPT_PERFORM_LOOPREDUCE },
    { "fcache", OPT_AUTO_FCACHE },
    { "inline-single", OPT_INLINE_SINGLEUSE },
    { "cse", OPT_PERFORM_CSE },
    { "remove-bss", OPT_REMOVE_HUB_BSS },
    { "casetable", OPT_CASETABLE },
    { "extrasmall", OPT_EXTRASMALL },
    { "bcmacros", OPT_MAKE_MACROS },
    { "special-functions", OPT_SPECIAL_FUNCS },
    { "cordic-reorder", OPT_CORDIC_REORDER},
    { "local-reuse", OPT_LOCAL_REUSE},
    { "all", OPT_FLAGS_ALL },
};
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

int ParseOptimizeString(AST *line, const char *str, int *flag_ptr)
{
    int flags = *flag_ptr;
    int notflag;
    int bits, i;
    char buf_base[80];
    char *buf;
    
    if (!(*str)) {
        // default to -O2
        *flag_ptr = DEFAULT_ASM_OPTS | EXTRA_ASM_OPTS;
        return 1;
    }
    while (str && *str) {
        buf = buf_base;
        str = GetOptionString(buf, sizeof(buf_base), str);
        if (*buf == '!' || *buf == '~') {
            buf++;
            notflag = 1;
        } else {
            notflag = 0;
        }
        if (*buf == 0) {
            continue;
        }
        /* basic values like -O1, -O2, etc */
        if (buf[1] == 0) {
            switch(buf[0]) {
            case '0':
                flags = 0;
                continue;
            case '1':
                flags = DEFAULT_ASM_OPTS;
                continue;
            case 's':
                flags = DEFAULT_ASM_OPTS | OPT_EXTRASMALL;
                continue;
            case '2':
            case '3':
            case '4':
            case '5':
                flags = DEFAULT_ASM_OPTS | EXTRA_ASM_OPTS;
                continue;
            default:
                ERROR(line, "Unrecognized asm option: %s", buf);
                return 0;
            }
        }
        bits = 0;
        for (i = 0; i < ARRAY_SIZE(optflag); i++) {
            if (!strcmp(optflag[i].name, buf)) {
                bits = optflag[i].bits;
                break;
            }
        }
        if (!bits) {
            ERROR(line, "Unrecognized optimization flag: %s", buf);
            return 0;
        }
        if (notflag) {
            flags &= ~bits;
        } else {
            flags |= bits;
        }
    }
    *flag_ptr = flags;
    return 1;
}

int
ParseWFlags(const char *flags)
{
    if (!strcmp(flags, "all")) {
        gl_warn_flags = WARN_ALL;
    } else if (!strcmp(flags, "error")) {
        gl_warnings_are_errors = 1;
    } else if (!strcmp(flags, "abs-paths")) {
        gl_useFullPaths = 1;
    } else if (!strncmp(flags, "max-errors=", 11)) {
        int n = strtol(flags+11, NULL, 10);
        if (n < 1) {
            fprintf(stderr, "Unrecognized value `%s' for max-errors (must be at least 1)\n", flags+11);
            return 0;
        }
        gl_max_errors = n;
    } else {
        return 0;
    }
    return 1;
}

//
// check to see if the only Spin function in P would
// start the cog code, in which case we can leave off
// the Spin code entirely
//
static int TrivialSpinFunction(Module *P)
{
    Function *pf;
    AST *body;
    AST *expr;
    
    if (!P || !P->functions) {
        // no Spin functions at all!
        return 1;
    }
    pf = P->functions;
    if (pf->next) {
        // more than one function
        return 0;
    }
    body = pf->body;
    if (!body) {
        // hmmm odd, nothing in the Spin code at all?
        // punt, we are confused
        return 0;
    }
    if (body->kind != AST_STMTLIST) {
        return 0;
    }
    if (body->right != 0) {
        // more than one statement
        return 0;
    }
    body = body->left;
    
    while (body && body->kind == AST_COMMENTEDNODE) {
        body = body->left;
    }
    if (!body) {
        // odd, no spin code? punt
        return 0;
    }
    if (body->kind != AST_COGINIT) {
        return 0;
    }
    body = body->left;
    // now check the arguments
    // we want:
    //   coginit(0, @addr, 0)
    // where "addr" is a label

    // check first argument
    if (!body || body->kind != AST_EXPRLIST) return 0;
    expr = body->left;
    body = body->right;

    // check for 0
    if (!IsConstExpr(expr) || EvalConstExpr(expr) != 0) return 0;

    // check second argument
    if (!body || body->kind != AST_EXPRLIST) return 0;
    expr = body->left;
    body = body->right;

    // check for @
    if (expr->kind != AST_ADDROF && expr->kind == AST_ABSADDROF) return 0;
    
    // check third argument
    if (!body || body->kind != AST_EXPRLIST) return 0;
    expr = body->left;
    body = body->right;

    // check for 0
    if (!IsConstExpr(expr) || EvalConstExpr(expr) != 0) return 0;

    return 1;
}

//
// check for special defines like -D_BAUD=nnn
//
void check_special_define(const char *name, const char *def)
{
    int val;
    if (!strcmp(name, "_BAUD")) {
        val = strtoul(def, NULL, 0);
        gl_default_baud = val;
    }
}

//
// parse character set definition
//
int ParseCharset(int *var, const char *name)
{
    if (!strcasecmp(name, "utf8") || !strcasecmp(name, "utf-8")) {
        *var = CHARSET_UTF8;
        return 1;
    }
    if (!strcasecmp(name, "latin1") || !strcasecmp(name, "latin-1")) {
        *var = CHARSET_LATIN1;
        return 1;
    }
    if (!strcasecmp(name, "parallax") || !strcasecmp(name, "oem")) {
        *var = CHARSET_PARALLAX;
        return 1;
    }
    if (!strcasecmp(name, "shiftjis") || !strcasecmp(name, "shift-jis")) {
        *var = CHARSET_SHIFTJIS;
        return 1;
    }
    return 0;
}
