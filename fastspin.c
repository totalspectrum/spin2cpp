/*
 * Spin to C/C++ translator
 * Copyright 2011-2018 Total Spectrum Software Inc.
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

// this version of the front end uses the openspin command line flags
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

extern int spinyydebug;

const char *gl_progname;
const char *gl_cc = NULL;
const char *gl_intstring = "int32_t";

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
AST *ast_type_word, *ast_type_long, *ast_type_byte;
AST *ast_type_float, *ast_type_string;
AST *ast_type_generic;
AST *ast_type_void;

const char *gl_outname = NULL;

double gl_start_time;

static void
PrintInfo(FILE *f, int bstcMode)
{
    if (bstcMode) {
        fprintf(f, "FastSpin Compiler v" VERSIONSTR " - Copyright 2011-2018 Total Spectrum Software Inc.\n");
        fprintf(f, "Compiled on: " __DATE__ "\n");
        
    } else {
        fprintf(f, "Propeller Spin/PASM Compiler 'FastSpin' (c) 2011-2018 Total Spectrum Software Inc.\n");
        fprintf(f, "Version " VERSIONSTR " Compiled on: " __DATE__ "\n");
    }
    fflush(f);
}

static void
Usage(FILE *f, int bstcMode)
{
    if (bstcMode) {
        fprintf(f, "Program usage :- %s (Options) Filename[.spin]\n", gl_progname);
    } else {
        fprintf(f, "usage: %s\n", gl_progname);
    }
    fprintf(f, "  [ -h ]              display this help\n");
    fprintf(f, "  [ -L or -I <path> ] add a directory to the include path\n");
    fprintf(f, "  [ -o ]             output filename\n");
    fprintf(f, "  [ -b ]             output binary file format\n");
    fprintf(f, "  [ -e ]             output eeprom file format\n");
    fprintf(f, "  [ -c ]             output only DAT sections\n");
    fprintf(f, "  [ -l ]             output DAT as a listing file\n");
    fprintf(f, "  [ -f ]             output list of file names\n");
    fprintf(f, "  [ -q ]             quiet mode (suppress banner and non-error text)\n");
    fprintf(f, "  [ -p ]             disable the preprocessor\n");
    fprintf(f, "  [ -D <define> ]    add a define\n");
    fprintf(f, "  [ -u ]             ignore for openspin compatibility (unused method elimination always enabled)\n");
    fprintf(f, "  [ -2 ]             compile for Prop2\n");
    fprintf(f, "  [ -O# ]            set optimization level:\n");
    fprintf(f, "          -O0 = no optimization\n");
    fprintf(f, "          -O1 = basic optimization\n");
    fprintf(f, "          -O2 = all optimization\n");
    fprintf(f, "  [ -w ]             compile for COG with Spin wrappers\n");
    fprintf(f, "  [ --code=cog ]     compile for COG mode instead of LMM\n");
    fprintf(f, "  [ --fcache=N ]     set FCACHE size to N (0 to disable)\n");
    fflush(stderr);
    exit(2);
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

double
getCurTime()
{
    clock_t tick = clock();
    return (double)tick / (double)CLOCKS_PER_SEC;
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
    int quiet = 0;
    int bstcMode = 0;
    Module *P;
    int retval = 0;
    struct flexbuf argbuf;
    time_t timep;
    int i;
    const char *outname = NULL;
    size_t eepromSize = 32768;
    int useEeprom = 0;
    const char *target = NULL;
    const char *listFile = NULL;
    
#if 0
    printf("fastspin: arguments are:\n");
    for (i = 0; i < argc; i++) {
        printf("[%s]\n", argv[i]);
    }
    fflush(stdout);
#endif    
    gl_start_time = getCurTime();
    
    /* Initialize the global preprocessor; we need to do this here
       so that the -D command line option can define preprocessor
       symbols. The rest of initialization happens after command line
       options have been parsed
    */
    InitPreprocessor();

    /* save our command line arguments and comments describing
       how we were run
    */
    flexbuf_init(&argbuf, 128);
    flexbuf_addstr(&argbuf, "automatically generated by fastspin v" VERSIONSTR " on ");
    time(&timep);
    flexbuf_addstr(&argbuf, asctime(localtime(&timep)));
    flexbuf_addchar(&argbuf, 0);
    gl_header1 = flexbuf_get(&argbuf);

    flexbuf_addstr(&argbuf, "command line: ");
    for (i = 0; i < argc; i++) {
        flexbuf_addstr(&argbuf, argv[i]);
        flexbuf_addchar(&argbuf, ' ');
    }
    flexbuf_addstr(&argbuf, "\n\n");
    flexbuf_addchar(&argbuf, 0);
    gl_header2 = flexbuf_get(&argbuf);
    
    gl_output = OUTPUT_ASM;
    gl_outputflags = OUTFLAGS_DEFAULT;
    
    allparse = NULL;
#ifdef DEBUG_YACC
    spinyydebug = 1;  /* turn on yacc debugging */
#endif
    /* parse arguments */
    if (argv[0] != NULL) {
        gl_progname = argv[0];
        argv++; --argc;
    }
    // check for name starting with "bstc"
    // also, if our name ends in "spin2" use Spin2 mode
    {
        const char *nameRoot;
        size_t n;
        nameRoot = gl_progname;
        while (*nameRoot != 0) nameRoot++;
        while (nameRoot > gl_progname && nameRoot[-1] != '/' && nameRoot[-1] != '\\') --nameRoot;
        
        if (strncmp(nameRoot, "bstc", 4) == 0) {
            bstcMode = 1;
        }
        n = strlen(nameRoot);
        if (n > 4) {
            if (strcmp(nameRoot + n - 5, "spin2") == 0) {
                gl_p2 = 1;
            } else if (n > 8 && strcmp(nameRoot + n - 9, "spin2.exe") == 0) {
                gl_p2 = 1;
            }
        }
    }
    gl_normalizeIdents = 1;
    compile = 1;
    outputMain = 1;
    outputBin = 1;
    outputAsm = 1;
    gl_optimize_flags = DEFAULT_ASM_OPTS;
    
    // put everything in HUB by default
    gl_outputflags &= ~OUTFLAG_COG_DATA;
    gl_outputflags &= ~OUTFLAG_COG_CODE;
    
    while (argv[0] && argv[0][0] != 0) {
        if (argv[0][0] != '-') {
            if (target) {
                Usage(stderr, bstcMode);
            }
            target = argv[0];
            argv++; argc--;
            continue;
        }
        if (!strcmp(argv[0], "-y")) {
            spinyydebug = 1;
            argv++; --argc;
        } else if (!strncmp(argv[0], "--data=", 7)) {
            if (!strcmp(argv[0]+7, "cog")) {
                gl_outputflags |= OUTFLAG_COG_DATA;
            } else if (!strcmp(argv[0]+7, "hub")) {
                gl_outputflags &= ~OUTFLAG_COG_DATA;
            } else {
                fprintf(stderr, "Unknown --data= choice: %s\n", argv[0]);
                Usage(stderr, bstcMode);
            }
            argv++; --argc;
        } else if (!strncmp(argv[0], "--fcache=", 9)) {
            gl_fcache_size = atoi(argv[0]+9);
            if (gl_fcache_size < 8) {
                gl_fcache_size = 0;
            }
            argv++; --argc;
        } else if (!strncmp(argv[0], "--code=", 7)) {
            if (!strcmp(argv[0]+7, "cog")) {
                gl_outputflags |= OUTFLAG_COG_CODE;
            } else if (!strcmp(argv[0]+7, "hub")) {
                gl_outputflags &= ~OUTFLAG_COG_CODE;
            } else {
                fprintf(stderr, "Unknown --code= choice: %s\n", argv[0]);
                Usage(stderr, bstcMode);
            }
            argv++; --argc;
        } else if (!strcmp(argv[0], "-w")) {
            gl_outputflags |= OUTFLAG_COG_CODE;
            gl_output = OUTPUT_COGSPIN;
            gl_debug = 1;
            compile = 0;
            outputMain = 0;
            outputBin = 0;
            argv++; --argc;
        } else if (!strcmp(argv[0], "-c") || !strncmp(argv[0], "--dat", 5)) {
            compile = 0;
            outputMain = 0;
            outputBin = 0;
            gl_output = OUTPUT_DAT;
            outputDat = 1;
            argv++; --argc;
        } else if (!strcmp(argv[0], "-l")) {
            gl_listing = 1;
            argv++; --argc;
        } else if (!strcmp(argv[0], "-f")) {
            outputFiles = 1;
            quiet = 1;
            argv++; --argc;
        } else if (!strcmp(argv[0], "-q")) {
            quiet = 1;
            argv++; --argc;
        } else if (!strcmp(argv[0], "-u")) {
            // ignore -u, we always eliminate unused methods
            argv++; --argc;
        } else if (!strcmp(argv[0], "-2")) {
            gl_p2 = 1;
            argv++; --argc;
        } else if (!strcmp(argv[0], "-h")) {
            PrintInfo(stdout, bstcMode);
            Usage(stdout, bstcMode);
            exit(0);
        } else if (!strncmp(argv[0], "--bin", 5) || !strcmp(argv[0], "-b")
                   || !strcmp(argv[0], "-e"))
        {
            compile = 1;
            outputMain = 1;
	    outputBin = 1;
            if (!strcmp(argv[0], "-e")) {
                useEeprom = 1;
            }
            argv++; --argc;
        } else if (!strcmp(argv[0], "-p")) {
            gl_preprocess = 0;
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
	    gl_outname = outname = strdup(opt);
        } else if (!strncmp(argv[0], "-g", 2)) {
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
            opt = strdup(opt);
            incpath = opt;
            pp_add_to_path(&gl_pp, incpath);
        } else if (!strncmp(argv[0], "-O", 2)) {
            // -O0 means no optimization
            // -O1 means default optimization
            // -O2 means extra optimization
            int flag = argv[0][2];
            if (flag == '0') {
                gl_optimize_flags = 0;
            } else if (flag == '1') {
                gl_optimize_flags = DEFAULT_ASM_OPTS;
            } else {
                gl_optimize_flags = DEFAULT_ASM_OPTS|EXTRA_ASM_OPTS;
            }
            argv++; --argc;
        } else {
            fprintf(stderr, "Unrecognized option: %s\n", argv[0]);
            Usage(stderr, bstcMode);
        }
    }
    
    if (!quiet) {
        PrintInfo(stdout, bstcMode);
    }
    if (target == NULL) {
        Usage(stderr, bstcMode);
    }

    /* tweak flags */
    if (gl_output == OUTPUT_COGSPIN) {
        gl_optimize_flags &= ~OPT_REMOVE_UNUSED_FUNCS;
    }
    /* add some predefined symbols */
    
    pp_define(&gl_pp, "__FASTSPIN__", str_(VERSION_MAJOR));
    pp_define(&gl_pp, "__SPINCVT__", str_(VERSION_MAJOR));
    if (gl_output == OUTPUT_ASM || gl_output == OUTPUT_COGSPIN) {
        pp_define(&gl_pp, "__SPIN2PASM__", "1");
    }
    if (gl_output == OUTPUT_CPP || gl_output == OUTPUT_C) {
        pp_define(&gl_pp, "__SPIN2CPP__", "1");
        if (gl_output == OUTPUT_CPP) {
            pp_define(&gl_pp, "__cplusplus", "1");
        }
    }
    if (gl_p2) {
        pp_define(&gl_pp, "__P2__", "1");
    }
    /* set up the binary offset */
    gl_dat_offset = -1; // by default offset is unknown
    if ( (gl_output == OUTPUT_DAT||gl_output == OUTPUT_ASM) && outputBin) {
        // a 32 byte spin header is prepended to binary output of dat
        // (but not in p2 mode)
        gl_dat_offset = gl_p2 ? 0 : 32;
    } else if (gl_output == OUTPUT_DAT && gl_gas_dat) {
        // GAS output for dat uses symbols, so @@@ is OK there
        gl_dat_offset = 0;
    }

    if (gl_listing && !(gl_output == OUTPUT_DAT)) {
        gl_srccomments = 1;
    }
    
    /* initialize the parser; we do that after command line processing
       so that command line options can influence it */
    Init();

    /* now actually parse the file */
    if (!quiet) {
        gl_printprogress = 1;
    }
    P = ParseTopFile(target, outputBin);

    if (outputFiles) {
        Module *Q;
        for (Q = allparse; Q; Q = Q->next) {
            printf("%s\n", Q->fullname);
        }
        return 0;
    }
    
    if (P) {
        /* do type checking and deduction */
        Module *Q;

        if (gl_errors > 0) {
            exit(1);
        }
        /* set up output file names */
        if (gl_listing) {
            listFile = ReplaceExtension(P->fullname, ".lst");
        }
    
        if (outputDat) {
            outname = gl_outname;
            if (gl_gas_dat) {
	        if (!outname) {
                    outname = ReplaceExtension(P->fullname, ".S");
                }
                OutputGasFile(outname, P);
            } else {
	        if (!outname) {
                    if (outputBin) {
                        if (useEeprom) {
                            outname = ReplaceExtension(P->fullname, ".eeprom");
                        } else {
                            outname = ReplaceExtension(P->fullname, ".binary");
                        }
                    } else if (listFile) {
                        outname = ReplaceExtension(P->fullname, ".lst");
                    } else {
                        outname = ReplaceExtension(P->fullname, ".dat");
                    }
                }
                if (bstcMode && !listFile) {
                    outname = ReplaceExtension(outname, ".binary");
                }
                if (listFile) {
                    OutputLstFile(listFile, P);
                }
                OutputDatFile(outname, P, outputBin);
                if (outputBin) {
                    DoPropellerChecksum(outname, useEeprom ? eepromSize : 0);
                }
            }
        } else if (outputAsm) {
            const char *binname = NULL;
            const char *asmname = NULL;
            if (compile) {
                binname = gl_outname;
                if (binname) {
                    asmname = ReplaceExtension(binname, gl_p2 ? ".p2asm" : ".pasm");
                } else {
                    if (useEeprom) {
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
            OutputAsmCode(asmname, P, outputMain);
            if (compile)  {
                if (gl_errors > 0) {
                    remove(binname);
                    if (listFile) {
                        remove(listFile);
                    }
                    exit(1);
                }
                gl_output = OUTPUT_DAT;
                Q = ParseTopFile(asmname, 1);
                if (gl_errors == 0) {
                    if (listFile) {
                        OutputLstFile(listFile, Q);
                    }
                    OutputDatFile(binname, Q, 1);
                    DoPropellerChecksum(binname, useEeprom ? eepromSize : 0);
                }
                if (!quiet) {
                    printf("Done.\n");
                    if (gl_errors == 0) {
                        PrintFileSize(binname);
                    }
                    if (bstcMode) {
                        int loc = 0;
                        double now = getCurTime();
                        for (Q = allparse; Q; Q = Q->next) {
                            loc += Q->L.lineCounter;
                        }
                        printf("Compiled %d Lines of Code in %.3f Seconds\n", loc, now - gl_start_time);
                    }
                }
            }
        } else {
            fprintf(stderr, "fastspin cannot convert to C\n");
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
