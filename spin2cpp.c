/*
 * Spin to C/C++ translator
 * Copyright 2011-2023 Total Spectrum Software Inc.
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

extern Module *allparse; /* declared in common.c */
extern int spinyydebug;

static void
Usage(void)
{
    fprintf(stderr, "Usage: %s [options] file.spin\n", gl_progname);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --asm:     output (user readable) PASM code\n");
    fprintf(stderr, "  --binary:  create binary file for download\n");
    fprintf(stderr, "  --cogspin: create PASM based Spin object (translate Spin to PASM)\n");
    fprintf(stderr, "  --ccode:   output C code instead of C++\n");
    fprintf(stderr, "  --cse:     perform common subexpression optimizations on C code\n");
    fprintf(stderr, "  --cc=CC:   use CC as the C++ compiler instead of PropGCC\n");
    fprintf(stderr, "  --code=x : PASM output only: control placement of code\n");
    fprintf(stderr, "             x can be cog (default) or hub (for LMM)\n");
    fprintf(stderr, "  --ctypes : use inferred pointer (and other) types in generated C/C++ code\n");
    fprintf(stderr, "  --data=x : PASM output only: control placement of data\n");
    fprintf(stderr, "             x can be cog or hub (default is hub)\n");
    fprintf(stderr, "  --dat:     output binary blob of DAT section only\n");
    fprintf(stderr, "  --eeprom:  create EEPROM binary file for download\n");
    fprintf(stderr, "  --elf:     create executable ELF file with propgcc\n");
    fprintf(stderr, "  --files:   print list of .cpp files to stdout\n");
    fprintf(stderr, "  --fixed:   use 16.16 fixed point in place of float\n");
    fprintf(stderr, "  --fcache=N: set size of FCACHE area\n");
    fprintf(stderr, "  --gas:     create inline assembly out of DAT area;\n");
    fprintf(stderr, "             with --dat, create gas .S file from DAT area\n");
    fprintf(stderr, "  --list:    produce a listing file\n");
    fprintf(stderr, "  --main:    include C++ main() function\n");
    fprintf(stderr, "  --noheader: skip the normal comment about spin2cpp version\n");
    fprintf(stderr, "  --nocse:   disable common subexpression optimizations on PASM code\n");
    fprintf(stderr, "  --noopt:   turn off all optimization in PASM output\n");
    fprintf(stderr, "  --nopre:   do not run preprocessor on the .spin file\n");
    fprintf(stderr, "  --nofcache: disable FCACHE (same as --fcache=0)\n");
    fprintf(stderr, "  --normalize: normalize case of all identifiers\n"); 
    fprintf(stderr, "  --p2:       use Propeller 2 instructions (experimental)\n");
    fprintf(stderr, "  --require:  require a specific version (or later) of spin2cpp\n");
    fprintf(stderr, "  --side:     create a SimpleIDE file for the C/C++ outputs\n");
    fprintf(stderr, "  -Dname=val: define a preprocessor symbol\n");
    fprintf(stderr, "  -g:         add debug info to output (original source for PASM output)\n");
    fprintf(stderr, "  -I dir:     add dir to the object search path\n");
    fprintf(stderr, "  -L dir:     same as -I\n");
    fprintf(stderr, "  -o file:    place final output in file\n");
    fprintf(stderr, "  -Wall:      enable all warnings\n");
    fprintf(stderr, "  -y:         debug parser\n");
    fprintf(stderr, "  --verbose:  print additional diagnostic messages\n");
    fprintf(stderr, "  --version:  print version and exit\n");
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
        ccompiler = gl_cc;
    }
    if (!ccompiler) {
        ccompiler = getenv("CC");
    }
    if (!ccompiler) {
        ccompiler = "propeller-elf-gcc";
    }
    appendToCmd(ccompiler);
}

int
main(int argc, const char **argv)
{
    int outputMain = 0;
    int outputDat = 0;
    int outputFiles = 0;
    int outputBin = 0;
    int outputAsm = 0;
    int outputSide = 0;
    int compile = 0;
    size_t eepromSize = 0;
    Module *P;
    int retval = 0;
    const char *cext = ".cpp";
    struct flexbuf argbuf;
    time_t timep;
    int i;
    const char *outname = NULL;
    const char *listFile = NULL;
    int wantcse = -1;

    gl_max_errors = 1;
    gl_src_charset = CHARSET_UTF8;
    gl_run_charset = CHARSET_UTF8;
    
    /* Initialize the global preprocessor; we need to do this here
       so that the -D command line option can define preprocessor
       symbols. The rest of initialization happens after command line
       options have been parsed
    */
    InitPreprocessor(argv);
    
    /* save our command line arguments and comments describing
       how we were run
    */
    flexbuf_init(&argbuf, 128);
    flexbuf_addstr(&argbuf, "automatically generated by spin2cpp v");
    flexbuf_addstr(&argbuf, VERSIONSTR);
    flexbuf_addstr(&argbuf, " on ");
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
    gl_output = OUTPUT_CPP;
    gl_outputflags = OUTFLAGS_DEFAULT;
    
    allparse = NULL;
#ifdef DEBUG_YACC
    spinyydebug = 1;  /* turn on yacc debugging */
#endif
    gl_optimize_flags = DEFAULT_ASM_OPTS;
    
    /* parse arguments */
    if (argv[0] != NULL) {
        gl_progname = argv[0];
        argv++; --argc;
    }
    while (argv[0] && argv[0][0] == '-') {
        if (!strcmp(argv[0], "-y")) {
            spinyydebug = 1;
            argv++; --argc;
        } else if (!strcmp(argv[0], "--main") || !strcmp(argv[0], "-main")) {
            outputMain = 1;
            argv++; --argc;
        } else if (!strncmp(argv[0], "--data=", 7)) {
            if (!strcmp(argv[0]+7, "cog")) {
                gl_outputflags |= OUTFLAG_COG_DATA;
            } else if (!strcmp(argv[0]+7, "hub")) {
                gl_outputflags &= ~OUTFLAG_COG_DATA;
            } else {
                fprintf(stderr, "Unknown --data= choice: %s\n", argv[0]);
                Usage();
            }
            argv++; --argc;
        } else if (!strncmp(argv[0], "--code=", 7)) {
            if (!strcmp(argv[0]+7, "cog")) {
                gl_outputflags |= OUTFLAG_COG_CODE;
            } else if (!strcmp(argv[0]+7, "hub")) {
                gl_outputflags &= ~OUTFLAG_COG_CODE;
            } else {
                fprintf(stderr, "Unknown --code= choice: %s\n", argv[0]);
                Usage();
            }
            argv++; --argc;
        } else if (!strcmp(argv[0], "--color")) {
            gl_colorize_output = true;
            argv++; --argc;
        } else if (!strcmp(argv[0], "--nocolor")) {
            gl_colorize_output = false;
            argv++; --argc;
        } else if (!strcmp(argv[0], "--verbose")) {
            gl_verbosity = 1;
            argv++; --argc;
        } else if (!strcmp(argv[0], "--dat") || (!compile && !strcmp(argv[0], "-c"))) {
            gl_output = OUTPUT_DAT;
            outputDat = 1;
            argv++; --argc;
        } else if (!strcmp(argv[0], "--noopt") ) {
            gl_optimize_flags = 0;
            wantcse = 0;
            argv++; --argc;
        } else if (!strcmp(argv[0], "--cse") ) {
            wantcse = 1;
            argv++; --argc;
        } else if (!strcmp(argv[0], "--nocse") ) {
            wantcse = 0;
            argv++; --argc;
        } else if (!strcmp(argv[0], "--asm") ) {
            outputAsm = 1;
            gl_output = OUTPUT_ASM;
            argv++; --argc;
            gl_optimize_flags = (DEFAULT_ASM_OPTS & ~OPT_REMOVE_UNUSED_FUNCS);
        } else if (!strcmp(argv[0], "--list")) {
            gl_listing = 1;
            argv++; --argc;
        } else if (!strcmp(argv[0], "--cogspin") ) {
            outputAsm = 1;
            gl_output = OUTPUT_COGSPIN;
            argv++; --argc;
            gl_optimize_flags |= DEFAULT_ASM_OPTS;
        } else if (!strcmp(argv[0], "--gas")) {
            gl_gas_dat = 1;
            argv++; --argc;
        } else if (!strcmp(argv[0], "--normalize") || !strcmp(argv[0], "-n")) {
            gl_normalizeIdents = 1;
            argv++; --argc;
        } else if (!strcmp(argv[0], "--p2")) {
            gl_p2 = DEFAULT_P2_VERSION;
            argv++; --argc;
        } else if (!strcmp(argv[0], "--side")) {
            outputSide = 1;
            outputMain = 1;
            argv++; --argc;
        } else if (!strcmp(argv[0], "--ccode")) {
            gl_output = OUTPUT_C;
            cext = ".c";
            argv++; --argc;
        } else if (!strcmp(argv[0], "--ctypes")) {
            gl_infer_ctypes = 1;
            argv++; --argc;
        } else if (!strncmp(argv[0], "--cc=", 5)) {
            gl_cc = argv[0] + 5;
            gl_intstring = "intptr_t"; // for 64 bit targets
            argv++; --argc;
        } else if (!strncmp(argv[0], "--optimize", 5)) {
            /* for debug purpose only: override optimize flags with hex */
            argv++; --argc;
            if (argv[0] == NULL) {
                fprintf(stderr, "Error: expected another argument after --optimize\n");
                exit(2);
            }
            ParseOptimizeString(NULL, argv[0], &gl_optimize_flags);
            argv++; --argc;
        } else if (!strncmp(argv[0], "--files", 7)) {
            outputFiles = 1;
            argv++; --argc;
        } else if (!strcmp(argv[0], "-Wall")) {
            gl_warn_flags = WARN_ALL;
            argv++; --argc;
        } else if (!strncmp(argv[0], "--version", 7) || !strcmp(argv[0], "-v")) {
            printf("Spin to C++ converter version %s\n", VERSIONSTR);
            exit(0);
        } else if (!strncmp(argv[0], "--require", 9)) {
            const char *reqStr;
            if (argv[0][9] == '=') {
                reqStr = argv[0]+10;
            } else {
                argv++; --argc;
                if (!argv[0]) {
                    Usage();
                }
                reqStr = argv[0];
            }
            CheckVersion(reqStr);
            argv++; --argc;
        } else if (!strcmp(argv[0], "--elf")) {
            compile = 1;
            outputMain = 1;
            argv++; --argc;
            appendCompiler(NULL);
            // gl_optimize_flags |= OPT_REMOVE_UNUSED_FUNCS; this can cause issues
        } else if (!strncmp(argv[0], "--catalina", 10)) {
            compile = 1;
            outputMain = 1;
            gl_output = OUTPUT_C;
            cext = ".c";
            argv++; --argc;
            appendCompiler("catalina");
        } else if (!strncmp(argv[0], "--bin", 5) || !strcmp(argv[0], "-b")
                   || !strncmp(argv[0], "--eep", 5) )
        {
            compile = 1;
            outputMain = 1;
	    outputBin = 1;
            if (gl_output == OUTPUT_ASM) {
                gl_optimize_flags |= OPT_REMOVE_UNUSED_FUNCS;
            } else if (gl_output == OUTPUT_COGSPIN) {
                fprintf(stderr, "--cogspin and --binary are incompatible\n");
                exit(1);
            } else {
                appendCompiler(NULL);
            }
            if (!strncmp(argv[0], "--eep", 5)) {
                eepromSize = 32768;
            } else {
                eepromSize = 0;
            }
            argv++; --argc;
        } else if (!strncmp(argv[0], "--nopre", 7)) {
            gl_preprocess = 0;
            argv++; --argc;
        } else if (!strncmp(argv[0], "--noheader", 9)) {
            free(gl_header1);
            free(gl_header2);
            gl_header1 = gl_header2 = NULL;
            argv++; --argc;
        } else if (!strncmp(argv[0], "--nofcache", 10)) {
            gl_fcache_size = 0;
            argv++; --argc;
        } else if (!strncmp(argv[0], "--fcache=", 9)) {
            gl_fcache_size = atoi(argv[0]+9);
            if (gl_fcache_size < 8) {
                gl_fcache_size = 0;
            }
            argv++; --argc;
        } else if (!strncmp(argv[0], "--fixed", 7)) {
            gl_fixedreal = 1;
            argv++; --argc;
	} else if (!strncmp(argv[0], "-o", 2)) {
	    const char *opt;
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
        } else if (!strcmp(argv[0], "-gbrk")) {
            argv++; --argc;
            gl_debug = 1;
            gl_brkdebug = 1;
        } else if (!strncmp(argv[0], "-g", 2)) {
            if (compile) {
                appendToCmd(argv[0]);
            }
            argv++; --argc;
            gl_debug = 1;
        } else if (!strncmp(argv[0], "-D", 2) || !strncmp(argv[0], "-C", 2)) {
            const char *opt = argv[0];
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
            name = strdup(opt);
            opt = name;
            while (*name && *name != '=')
                name++;
            if (*name) {
                *name++ = 0;
            } else {
                name = "1";
            }
            pp_define(&gl_pp, opt, name);
        } else if (!strncmp(argv[0], "-L", 2) || !strncmp(argv[0], "-I", 2)) {
            const char *opt = argv[0];
            const char *incpath;
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
                || !strncmp(argv[0], "-sav", 4)
                )
            {
                appendToCmd(argv[0]); argv++; --argc;
            } else if (!strncmp(argv[0], "-u", 2)
                       || !strncmp(argv[0], "-D", 2)
                )
            {
                const char *opt = argv[0];
                appendToCmd(opt); argv++; --argc;
                if (opt[2] == 0) {
                    if (argv[0] == NULL) {
                        fprintf(stderr, "Error: expected another argument after %s option\n", opt);
                        exit(2);
                    }
                    appendToCmd(argv[0]); argv++; --argc;
                }
            } else {
                fprintf(stderr, "Unrecognized option: %s\n", argv[0]);
                Usage();
            }
        } else {
            fprintf(stderr, "Unrecognized option: %s\n", argv[0]);
            Usage();
        }
    }
    if (wantcse < 0) {
        switch (gl_output) {
        case OUTPUT_C:
        case OUTPUT_CPP:
            wantcse = 0;
            break;
        default:
            wantcse = 1;
            break;
        }
    }
    if (!wantcse) {
        gl_optimize_flags &= ~OPT_PERFORM_CSE;
    }
    
    if (argv[0] == NULL || (argc != 1 && !compile)) {
        fprintf(stderr, "Spin to C++ converter version %s\n", VERSIONSTR);
        Usage();
    }

    /* add some predefined symbols */
    // add default path, if applicable
    if (!gl_nostdlib) {
        pp_add_to_path(&gl_pp, DefaultIncludeDir());
    }
    
    if (gl_p2) {
        pp_define(&gl_pp, "__propeller__", "2");
    } else {
        pp_define(&gl_pp, "__propeller__", "1");
    }
    pp_define(&gl_pp, "__FASTSPIN__", str_(VERSION_MAJOR));
    pp_define(&gl_pp, "__FLEXSPIN__", str_(VERSION_MAJOR));
    pp_define(&gl_pp, "__FLEXBASIC__", str_(VERSION_MAJOR));
    pp_define(&gl_pp, "__FLEXC__", str_(VERSION_MAJOR));
    pp_define(&gl_pp, "__SPIN2X__", str_(VERSION_MAJOR)); // deprecated
    pp_define(&gl_pp, "__SPINCVT__", str_(VERSION_MAJOR));
    if (gl_output == OUTPUT_ASM || gl_output == OUTPUT_COGSPIN) {
        pp_define(&gl_pp, "__SPIN2PASM__", "1");
    }
    if (gl_output == OUTPUT_CPP || gl_output == OUTPUT_C) {
        pp_define(&gl_pp, "__SPIN2CPP__", "1");
    }
    pp_define(&gl_pp, "__ILP32__", "1");
    if (gl_p2) {
        pp_define(&gl_pp, "__P2__", "1");
        pp_define(&gl_pp, "__propeller2__", "1");
    } else {
        pp_define(&gl_pp, "__P1__", "1");
    }
    /* set up the binary offset */
    gl_dat_offset = -1; // by default offset is unknown
    if ( (gl_output == OUTPUT_DAT||gl_output == OUTPUT_ASM) && outputBin) {
        // a 24 byte spin header is prepended to binary output of dat
        // (but not in P2)
        gl_dat_offset = gl_p2 ? 0 : DEFAULT_P1_DAT_OFFSET;
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
    /* NOTE: only a single file is permitted */
    P = ParseTopFiles(&argv[0], 1, outputBin);
#if 1
    if (compile && argc > 1) {
        /* append the remaining arguments to the command line */
        for (i = 1; i < argc; i++) {
            appendToCmd(argv[i]);
        }
    }
#endif
    if (P) {
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
                        if (eepromSize) {
                            outname = ReplaceExtension(P->fullname, ".eeprom");
                        } else {
                            outname = ReplaceExtension(P->fullname, ".binary");
                        }
                    } else {
                        outname = ReplaceExtension(P->fullname, ".dat");
                    }
                }
                if (listFile) {
                    OutputLstFile(listFile, P);
                }
                OutputDatFile(outname, P, outputBin);
                if (outputBin) {
                    DoPropellerPostprocess(outname, eepromSize);
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
                    if (eepromSize) {
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
                    if (gl_p2) {
                        asmname = ReplaceExtension(P->fullname, ".cog.spin2");
                    } else {
                        asmname = ReplaceExtension(P->fullname, ".cog.spin");
                    }
                } else {
                    asmname = ReplaceExtension(P->fullname, gl_p2 ? ".p2asm" : ".pasm");
                }
            }
            OutputAsmCode(asmname, P, outputMain);
            if (compile) {
                if (gl_errors > 0) {
                    remove(binname);
                    if (listFile) {
                        remove(listFile);
                    }
                    exit(1);
                }
                gl_output = OUTPUT_DAT;
                Q = ParseTopFiles(&asmname, 1, 1);
                if (gl_errors == 0) {
                    if (listFile) {
                        OutputLstFile(listFile, Q);
                    }
                    OutputDatFile(binname, Q, 1);
                    DoPropellerPostprocess(binname, eepromSize);
                }
                if (gl_errors > 0) {
                    remove(binname);
                    exit(1);
                }
            }
        } else {
            /* compile any sub-objects needed */
            for (Q = allparse; Q; Q = Q->next) {
                const char *outfilename;
                if (gl_outname) {
                    if (Q != P || compile) {
                        outfilename = ReplaceDirectory(Q->basename, gl_outname);
                    } else {
                        outfilename = gl_outname;
                    }
                } else {
                    outfilename = Q->basename;
                }
                Q->outfilename = outfilename;
                OutputCppCode(outfilename, Q, outputMain && Q == P);
                if (outputFiles) {
                    printf("%s%s\n", Q->basename, cext);
                }
                if (compile) {
                    appendToCmd(outfilename);
                    appendWithoutSpace(cext, 1);
                }
            }
            if (compile && gl_errors == 0) {
                const char *binname;
                const char *elfname;

                if (gl_outname == NULL) {
                    elfname = ReplaceExtension(argv[0], ".elf");
                    if (eepromSize) {
                        binname = ReplaceExtension(elfname, ".eeprom");
                    } else {
                        binname = ReplaceExtension(elfname, ".binary");
                    }
                } else if (outputBin) {
                    elfname = ReplaceExtension(gl_outname, ".elf");
                    binname = gl_outname;
                } else {
                    elfname = gl_outname;
                    binname = NULL;
                }
                appendToCmd("-o");
                appendToCmd(elfname);

                //printf("... executing [%s]\n", cmdline);
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
                    if (retval == 0) {
                        remove(elfname);
                        retval = DoPropellerPostprocess(binname, eepromSize);
                    } else {
                        fprintf(stderr, "command `%s' returned %d\n", cmdline, retval);
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

    if (outputSide) {
        FILE *f;
        Module *Q;
        if (gl_output != OUTPUT_C && gl_output != OUTPUT_CPP) {
            fprintf(stderr, "Skipping --side output because C code not generated\n");
            exit(1);
        }
        outname = gl_outname;
        if (!outname) {
            outname = argv[0];
        }
        outname = ReplaceExtension(outname, ".side");
        f = fopen(outname, "w");
        if (!f) {
            perror(outname);
            exit(1);
        }
        for (Q = allparse; Q; Q = Q->next) {
            fprintf(f, "%s%s\n", Q->basename, cext);
            fprintf(f, "%s.h\n", Q->basename);
        }
        fprintf(f, ">compiler=%s\n", gl_output == OUTPUT_C ? "C" : "C++");
        fprintf(f, ">memtype=cmm main ram compact\n");
        fprintf(f, ">optimize=-Os\n");
        fprintf(f, ">-fno-exceptions\n");
        fprintf(f, ">-fno-rtti\n");
        fclose(f);
    }
    return retval;
}
