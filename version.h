#ifndef VERSION_H_
#define VERSION_H_

#define str__(x) #x
#define str_(x) str__(x)

#define VERSION_MAJOR 6
#define VERSION_MINOR 4
#define VERSION_REV   0
//#define BETA "-beta"

#define VERSIONSTR version_string

#ifndef TCL_SRC
extern char version_string[];

extern void CheckVersion(const char *string);
#endif

#endif
