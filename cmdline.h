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
    int eepromSize;
    int quiet;
    int bstcMode;
    const char *outname;
#define MAX_FILES_ON_CMD_LINE 1024
    int file_argc;
    const char *file_argv[MAX_FILES_ON_CMD_LINE];
} CmdLineOptions;

void InitializeSystem(CmdLineOptions *opts, const char **argv);
int ProcessCommandLine(CmdLineOptions *opts);

#endif
