#ifndef VERSION_H_
#define VERSION_H_

#define str__(x) #x
#define str_(x) str__(x)

#define VERSION_MAJOR 3
#define VERSION_MINOR 9
#define VERSION_REV   18
//#define BETA "-beta"

extern char version_string[];

#define VERSIONSTR version_string

extern void CheckVersion(const char *string);

#endif
