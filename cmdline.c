/*
 * Spin to C/C++ translator
 * Copyright 2011-2020 Total Spectrum Software Inc.
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
#include "cmdline.h"

const char *gl_progname;
const char *gl_cc = NULL;
const char *gl_intstring = "int32_t";

const char *gl_outname = NULL;

void InitializeSystem(CmdLineOptions *cmd, const char **argv)
{
    memset(cmd, 0, sizeof(*cmd));
    cmd->eepromSize = 32768;
    InitPreprocessor(argv);
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
        static char quoted_version[256];
        sprintf(quoted_version, "\"%s\"", version_string);
        pp_define(&gl_pp, "__VERSION__", quoted_version);
    }
    if (gl_exit_status) {
        pp_define(&gl_pp, "__EXIT_STATUS__", "1");
    }
    if (gl_output == OUTPUT_ASM || gl_output == OUTPUT_COGSPIN) {
        pp_define(&gl_pp, "__SPIN2PASM__", "1");
    }
    if (gl_output == OUTPUT_CPP || gl_output == OUTPUT_C) {
        pp_define(&gl_pp, "__SPIN2CPP__", "1");
        if (gl_output == OUTPUT_CPP) {
            pp_define(&gl_pp, "__cplusplus", "1");
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
    /* set up the binary offset */
    gl_dat_offset = -1; // by default offset is unknown
    if ( (gl_output == OUTPUT_DAT||gl_output == OUTPUT_ASM) && cmd->outputBin) {
        // a 32 byte spin header is prepended to binary output of dat
        // (but not in p2 mode)
        gl_dat_offset = gl_p2 ? 0 : 32;
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
        Module *Q;
        int compile_original = 0;
        
        if (gl_errors > 0) {
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
                    DoPropellerChecksum(cmd->outname, cmd->useEeprom ? cmd->eepromSize : 0);
                }
            }
        } else if (cmd->outputAsm) {
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
            if (P->functions == NULL) {
                // we can just assemble the .spin file directoy
                asmname = strdup(P->fullname);
                compile_original = 1;
            } else {
                OutputAsmCode(asmname, P, cmd->outputMain);
            }
            if (cmd->compile)  {
                if (gl_errors > 0) {
                    remove(binname);
                    if (listFile) {
                        remove(listFile);
                    }
                    exit(1);
                }
                gl_output = OUTPUT_DAT;
                gl_caseSensitive = !compile_original;
                Q = ParseTopFiles(&asmname, 1, 1);
                if (gl_errors == 0) {
                    if (listFile) {
                        OutputLstFile(listFile, Q);
                    }
                    OutputDatFile(binname, Q, 1);
                    DoPropellerChecksum(binname, cmd->useEeprom ? cmd->eepromSize : 0);
                }
                if (!cmd->quiet) {
                    printf("Done.\n");
                    if (gl_errors == 0) {
                        PrintFileSize(binname);
                    }
                }
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
            case 's':
                flags = DEFAULT_ASM_OPTS;
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
