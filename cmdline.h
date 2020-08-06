#ifndef CMDLINE_H
#define CMDLINE_H

extern int spinyydebug;

extern const char *gl_progname;
extern const char *gl_cc;
extern const char *gl_intstring;

extern const char *gl_outname;

typedef struct CmdLineOptions {
    int outputMain;
    int outputDat;
    int outputFiles;
    int outputBin;
    int outputAsm;
    int compile;
    int useEeprom;
    int quiet;
    const char *outname;
} CmdLineOptions;

void InitializeSystem(CmdLineOptions *opts, const char **argv);
void ProcessCommandLine(CmdLineOptions *opts);

#endif
