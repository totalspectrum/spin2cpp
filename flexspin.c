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

// this version of the front end uses the openspin command line flags
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>
#include "spinc.h"
#include "preprocess.h"
#include "version.h"
#include "cmdline.h"

//#define DEBUG_YACC

extern int yyparse(void);

double gl_start_time;

static void
PrintInfo(FILE *f, int bstcMode)
{
    if (bstcMode) {
        fprintf(f, "FlexSpin Compiler v%s - Copyright 2011-2023 Total Spectrum Software Inc. and contributors\n", VERSIONSTR);
        fprintf(f, "Compiled on: " __DATE__ "\n");
        
    } else {
        fprintf(f, "Propeller Spin/PASM Compiler 'FlexSpin' (c) 2011-2023 Total Spectrum Software Inc. and contributors\n");
        fprintf(f, "Version %s Compiled on: " __DATE__ "\n", VERSIONSTR);
    }
    fflush(f);
}

static void
Usage(FILE *f, int bstcMode)
{
    if (bstcMode) {
        fprintf(f, "Program usage :- %s (Options) Filename[.spin]\n", gl_progname);
    } else {
        fprintf(f, "usage: %s [options] filename.spin | filename.bas\n", gl_progname);
    }
    fprintf(f, "  [ -h ]              display this help\n");
    fprintf(f, "  [ -L or -I <path> ] add a directory to the include path\n");
    fprintf(f, "  [ -o <name> ]      set output filename to <name>\n");
    fprintf(f, "  [ -b ]             output binary file format\n");
    fprintf(f, "  [ -e ]             output eeprom file format\n");
    fprintf(f, "  [ -c ]             output only DAT sections\n");
    fprintf(f, "  [ -l ]             output DAT as a listing file\n");
    fprintf(f, "  [ -f ]             output list of file names\n");
    fprintf(f, "  [ -g ]             enable debug statements\n");
    fprintf(f, "  [ -q ]             quiet mode (suppress banner and non-error text)\n");
    fprintf(f, "  [ -p ]             disable the preprocessor\n");
    fprintf(f, "  [ -D <define> ]    add a define\n");
    fprintf(f, "  [ -u ]             ignore for openspin compatibility (unused method elimination always enabled)\n");
    fprintf(f, "  [ -2 ]             compile for Prop2\n");
    fprintf(f, "  [ -2nu ]           compile for Prop2 with Nu interpreter\n");
    fprintf(f, "  [ -O# ]            set optimization level:\n");
    fprintf(f, "          -O0 = no optimization\n");
    fprintf(f, "          -O1 = basic optimization\n");
    fprintf(f, "          -O2 = all optimization\n");
    fprintf(f, "  [ -H nnnn ]        set starting hub address\n");
    fprintf(f, "  [ -E ]             skip initial coginit code (usually used with -H)\n");
    fprintf(f, "  [ -w ]             compile for COG with Spin wrappers\n");
    fprintf(f, "  [ -Wall ]          enable warnings for language extensions and other features\n");
    fprintf(f, "  [ -Werror ]        make warnings into errors\n");
    fprintf(f, "  [ -Wabs-paths ]    print absolute paths for file names in errors/warnings\n");
    fprintf(f, "  [ -Wmax-errors=N ] allow at most N errors in a pass before stopping\n");
    fprintf(f, "  [ -C ]             enable case sensitive mode\n");
    fprintf(f, "  [ -x ]             capture program exit code (for testing)\n");
    //fprintf(f, "  [ -z ]             compress code\n");
    fprintf(f, "  [ --charset=xxx ]  set character set for runtime\n");
    fprintf(f, "           xxx is one of utf8, latin1, shiftjis, or parallax\n");
    fprintf(f, "  [ --code=cog ]     compile for COG mode instead of LMM\n");
    fprintf(f, "  [ --interp=rom ]   compile bytecodes for P1 ROM interpreter (alpha feature!)\n");
    fprintf(f, "  [ --interp=nu ]    compile bytecodes for NuCode interpreter (alpha feature!)\n");
    fprintf(f, "  [ --fcache=N ]     set FCACHE size to N (0 to disable)\n");
    fprintf(f, "  [ --fixedreal ]    use 16.16 fixed point in place of floats\n");
    fprintf(f, "  [ --lmm=xxx ]      use alternate LMM implementation for P1\n");
    fprintf(f, "           xxx = orig uses original flexspin LMM\n");
    fprintf(f, "           xxx = slow uses traditional (slow) LMM\n");
    fprintf(f, "  [ --nostdlib]      skip searching in the standard library location for include files\n");
    fprintf(f, "  [ --sizes]         print code and interpreter sizes\n");
    fprintf(f, "  [ --tabs=N ]       assume tabs are set every N spaces for indentation purposes\n");
    fprintf(f, "  [ --verbose ]      print additional diagnostic messages (for debugging the compiler)\n");
    fprintf(f, "  [ --version ]      just show compiler version\n");
    fprintf(f, "  [ --zip ]          create zip archive of source files\n");
    
    fflush(stderr);
    exit(2);
}

double
getCurTime()
{
    clock_t tick = clock();
    return (double)tick / (double)CLOCKS_PER_SEC;
}


int
main(int argc, const char **argv)
{
    static CmdLineOptions cmd_base;
    CmdLineOptions *cmd = &cmd_base;
    int result;
    bool optimizeDefault = true;
    
    int retval = 0;
    struct flexbuf argbuf;
    time_t timep;
    int i;
    
    gl_start_time = getCurTime();

    InitializeSystem(cmd, argv);
    
    /* save our command line arguments and comments describing
       how we were run
    */
    flexbuf_init(&argbuf, 128);
    flexbuf_printf(&argbuf, "automatically generated by flexspin v %s on ", VERSIONSTR);
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
        char *default_include;
        size_t n;
        nameRoot = gl_progname;
        while (*nameRoot != 0) nameRoot++;
        while (nameRoot > gl_progname && nameRoot[-1] != '/' && nameRoot[-1] != '\\') --nameRoot;
        if (nameRoot > gl_progname) {
            n = nameRoot - gl_progname;
            default_include = (char *)malloc(n + 32);
            strncpy(default_include, gl_progname, n);
            strcpy(default_include+n, "../include");
            pp_add_to_path(&gl_pp, default_include);
        }
        if (strncmp(nameRoot, "bstc", 4) == 0) {
            cmd->bstcMode = 1;
        }
        n = strlen(nameRoot);
        if (n > 4) {
            if (strcmp(nameRoot + n - 5, "spin2") == 0) {
                gl_p2 = DEFAULT_P2_VERSION;
            } else if (n > 8 && strcmp(nameRoot + n - 9, "spin2.exe") == 0) {
                gl_p2 = DEFAULT_P2_VERSION;
            }
        }
    }
    gl_normalizeIdents = 0;
    cmd->compile = 1;
    cmd->outputMain = 1;
    cmd->outputBin = 1;
    cmd->outputAsm = 1;
    
    // put everything in HUB by default
    gl_outputflags &= ~OUTFLAG_COG_DATA;
    gl_outputflags &= ~OUTFLAG_COG_CODE;
    
    while (argv[0] && argv[0][0] != 0) {
        if (argv[0][0] != '-') {
            if (cmd->file_argc >= MAX_FILES_ON_CMD_LINE) {
                fprintf(stderr, "too many input files\n");
                exit(1);
            }
            cmd->file_argv[cmd->file_argc++] = argv[0];
            --argc; ++argv;
            continue;
        }
        if (!strcmp(argv[0], "-y")) {
            spinyydebug = 1;
            argv++; --argc;
        } else if (!strncmp(argv[0],"--interp=",9)) {
            if (!strcmp(argv[0]+9,"rom")) {
                gl_interp_kind = INTERP_KIND_P1ROM;
            } else if (!strcmp(argv[0]+9,"nu")) {
                gl_interp_kind = INTERP_KIND_NUCODE;
            } else {
                fprintf(stderr, "Unknown --interp= choice: %s\n", argv[0]);
                Usage(stderr, cmd->bstcMode);
            }
            gl_output = OUTPUT_BYTECODE;
            cmd->outputBytecode = 1;
            cmd->outputAsm = 0;
            argv++; --argc;
        } else if (!strncmp(argv[0], "--data=", 7)) {
            if (!strcmp(argv[0]+7, "cog")) {
                gl_outputflags |= OUTFLAG_COG_DATA;
            } else if (!strcmp(argv[0]+7, "hub")) {
                gl_outputflags &= ~OUTFLAG_COG_DATA;
            } else {
                fprintf(stderr, "Unknown --data= choice: %s\n", argv[0]);
                Usage(stderr, cmd->bstcMode);
            }
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
        } else if (!strncmp(argv[0], "--code=", 7)) {
            if (!strcmp(argv[0]+7, "cog")) {
                gl_outputflags |= OUTFLAG_COG_CODE;
            } else if (!strcmp(argv[0]+7, "hub")) {
                gl_outputflags &= ~OUTFLAG_COG_CODE;
            } else {
                fprintf(stderr, "Unknown --code= choice: %s\n", argv[0]);
                Usage(stderr, cmd->bstcMode);
            }
            argv++; --argc;
        } else if (!strncmp(argv[0], "--lmm=", 6)) {
            const char *lmmtype = argv[0]+6;
            if (!strcmp(lmmtype, "orig")) {
                gl_lmm_kind = LMM_KIND_ORIG;
            } else if (!strcmp(lmmtype, "slow")) {
                gl_lmm_kind = LMM_KIND_SLOW;
                gl_fcache_size = 0;
            } else if (!strcmp(lmmtype, "trace")) {
                gl_lmm_kind = LMM_KIND_TRACE;
            } else if (!strcmp(lmmtype, "cache")) {
                gl_lmm_kind = LMM_KIND_CACHE;
            } else {
                fprintf(stderr, "Unknown --lmm= choice: %s\n", lmmtype);
                Usage(stderr, cmd->bstcMode);
            }
            argv++; --argc;
        } else if (!strcmp(argv[0], "--relocatable")) {
            gl_relocatable = 1;
            fprintf(stderr, "WARNING: --relocatable not implemented yet\n");
            argv++; --argc;
        } else if (!strncmp(argv[0], "--tabs=", 6)) {
            const char *tabsetting = argv[0]+7;
            int stops = strtoul(tabsetting, NULL, 0);
            if (stops) {
                gl_tab_stops = stops;
            } else {
                fprintf(stderr, "Expecting integer for --tabs=%s\n", tabsetting);
                Usage(stderr, cmd->bstcMode);
            }
            argv++; --argc;
        } else if (!strcmp(argv[0], "--color")) {
            gl_colorize_output = true;
            argv++; --argc;
        } else if (!strcmp(argv[0], "--nocolor")) {
            gl_colorize_output = false;
            argv++; --argc;
        } else if (!strcmp(argv[0], "--nostdlib")) {
            gl_nostdlib = 1;
            argv++; --argc;
        } else if (!strcmp(argv[0], "--verbose")) {
            gl_verbosity = 1;
            argv++; --argc;
        } else if (!strcmp(argv[0], "--sizes")) {
            cmd->printSizes = 1;
            argv++; --argc;
        } else if (!strcmp(argv[0], "-w")) {
            gl_output = OUTPUT_COGSPIN;
            gl_debug = 1;
            cmd->compile = 0;
            cmd->outputMain = 0;
            cmd->outputBin = 0;
            argv++; --argc;
        } else if (!strcmp(argv[0], "-c") || !strncmp(argv[0], "--dat", 5)) {
            cmd->compile = 0;
            cmd->outputMain = 0;
            cmd->outputBin = 0;
            gl_output = OUTPUT_DAT;
            cmd->outputDat = 1;
            argv++; --argc;
        } else if (!strcmp(argv[0], "-C")) {
            gl_caseSensitive = 1;
            argv++; --argc;
        } else if (!strcmp(argv[0], "-E")) {
            gl_no_coginit = 1;
            argv++; --argc;
        } else if (!strcmp(argv[0], "-l")) {
            gl_listing = 1;
            argv++; --argc;
        } else if (!strcmp(argv[0], "--test-listing")) {
            gl_listing = -1;
            argv++; --argc;
        } else if (!strcmp(argv[0], "-f")) {
            cmd->outputFiles = 1;
            cmd->quiet = 1;
            argv++; --argc;
        } else if (!strcmp(argv[0], "-q")) {
            cmd->quiet = 1;
            argv++; --argc;
        } else if (!strcmp(argv[0], "-u")) {
            // ignore -u, we always eliminate unused methods
            argv++; --argc;
        } else if (!strcmp(argv[0], "-1")) {
            gl_p2 = 0;
            argv++; --argc;
        } else if (!strcmp(argv[0], "-1bc")) {
            gl_p2 = 0;
            gl_interp_kind = INTERP_KIND_P1ROM;
            gl_output = OUTPUT_BYTECODE;
            cmd->outputBytecode = 1;
            cmd->outputAsm = 0;
            argv++; --argc;
        } else if (!strcmp(argv[0], "-2nu")) {
            gl_p2 = DEFAULT_P2_VERSION;
            gl_interp_kind = INTERP_KIND_NUCODE;
            gl_output = OUTPUT_BYTECODE;
            cmd->outputBytecode = 1;
            cmd->outputAsm = 0;
            argv++; --argc;
        } else if (!strncmp(argv[0], "-2", 2)) {
            gl_p2 = DEFAULT_P2_VERSION;
            if (argv[0][2] >= 'a' && argv[0][2] <= 'z') {
                gl_p2 = argv[0][2] - 'a' + 1;
                if (gl_p2 == 1) {
                    fprintf(stderr, "warning: -2a option not officially supported\n");
                }
            }
            argv++; --argc;
        } else if (!strcmp(argv[0], "-h")) {
            PrintInfo(stdout, cmd->bstcMode);
            Usage(stdout, cmd->bstcMode);
            exit(0);
        } else if (!strcmp(argv[0], "--version")) {
            PrintInfo(stdout, cmd->bstcMode);
            exit(0);
        } else if (!strcmp(argv[0], "--zip")) {
            cmd->compile = 0;
            gl_output = OUTPUT_ZIP;
            argv++; --argc;
        } else if (!strncmp(argv[0], "--bin", 5) || !strcmp(argv[0], "-b")
                   || !strcmp(argv[0], "-e"))
        {
            cmd->compile = 1;
            cmd->outputMain = 1;
	    cmd->outputBin = 1;
            if (!strcmp(argv[0], "-e")) {
                cmd->useEeprom = 1;
            }
            argv++; --argc;
        } else if (!strcmp(argv[0], "-p")) {
            gl_preprocess = 0;
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
	    gl_outname = cmd->outname = strdup(opt);
        } else if (!strcmp(argv[0], "-gbrk")) {
            argv++; --argc;
            gl_debug = 1;
            gl_brkdebug = 1;
        } else if (!strncmp(argv[0], "-g", 2)) {
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
            check_special_define(opt, name);
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
            opt = strdup(opt);
            incpath = opt;
            pp_add_to_path(&gl_pp, incpath);
        } else if (!strncmp(argv[0], "-O", 2)) {
            // -O0 means no optimization
            // -O1 means default optimization
            // -O2 means extra optimization
            const char *flagstr = &argv[0][2];
            ParseOptimizeString(NULL, flagstr, &gl_optimize_flags);
            optimizeDefault = false;
            argv++; --argc;
        } else if (!strncmp(argv[0], "-z", 2)) {
            // -z0 means no compression (default)
            // -z1 means compress
            // other z values reserved
            int flag = argv[0][2];
            if (flag == '0') {
                gl_compress = 0;
            } else if (flag == '1' || flag == 0) {
                gl_compress = 1;
            } else {
                fprintf(stderr, "-z option %c is not supported\n", flag);
                Usage(stderr, cmd->bstcMode);
            }
            argv++; --argc;
        } else if (!strncmp(argv[0], "-W", 2) && argv[0][2]) {
            // -W alone is for "wrap"
            // -Wall means enable all warnings
            // other -W values reserved
            const char *flags = &argv[0][2];
            if (!ParseWFlags(flags)) {
                fprintf(stderr, "-W option %s is not supported\n", flags);
                Usage(stderr, cmd->bstcMode);
            }
            argv++; --argc;
        } else if (!strncmp(argv[0], "--charset=", 10)) {
            if (!ParseCharset(&gl_run_charset, argv[0]+10)) {
                fprintf(stderr, "Unknown character set in %s\n", argv[0]);
                Usage(stderr, cmd->bstcMode);
            }
            argv++; --argc;
        } else if (!strncmp(argv[0], "-H", 2)) {
            // set ub address
            const char *addr;
            if (argv[0][2] != 0) {
                addr = &argv[0][2];
            } else {
                argv++;
                --argc;
                addr = argv[0];
            }
            gl_hub_base = strtoul(addr, NULL, 0);
            if (gl_hub_base == 0) {
                fprintf(stderr, "Warning: hub base set to 0\n");
            }
            argv++; --argc;
        } else if (!strcmp(argv[0], "-x")) {
            argv++; --argc;
            gl_cenv_flags = 0xff;  // explicitly asked for testing
        } else {
            fprintf(stderr, "Unrecognized option: %s\n", argv[0]);
            Usage(stderr, cmd->bstcMode);
        }
    }

    if (gl_output == OUTPUT_COGSPIN && !gl_p2) {
        gl_outputflags |= OUTFLAG_COG_CODE;
    }
    // add default path, if applicable
    if (!gl_nostdlib) {
        pp_add_to_path(&gl_pp, DefaultIncludeDir());
    }
    
    if (optimizeDefault) {
        if (cmd->outputBytecode) {
            gl_optimize_flags = DEFAULT_BYTECODE_OPTS;
        } else {
            gl_optimize_flags = DEFAULT_ASM_OPTS;
        }
    }
    
    if (!cmd->quiet) {
        PrintInfo(stdout, cmd->bstcMode);
    }
    if (cmd->file_argc == 0) {
        Usage(stderr, cmd->bstcMode);
    }

    /* tweak flags */
    result = ProcessCommandLine(cmd);
    if (result) {
        return result;
    }
    if (cmd->bstcMode && cmd->compile) {
        int loc = 0;
        Module *Q;
        double now = getCurTime();
        for (Q = allparse; Q; Q = Q->next) {
            loc += Q->Lptr->lineCounter;
        }
        printf("Compiled %d Lines of Code in %.3f Seconds\n", loc, now - gl_start_time);
    }
    if (gl_errors > 0) {
        exit(1);
    }
    return retval;
}
