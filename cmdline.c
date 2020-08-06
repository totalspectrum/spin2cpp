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
    InitPreprocessor(argv);
}

void ProcessCommandLine(CmdLineOptions *cmd)
{
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

}
