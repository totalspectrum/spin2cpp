#ifndef VERSION_H_
#define VERSION_H_

#define str__(x) #x
#define str_(x) str__(x)

#define VERSION_MAJOR 3
#define VERSION_MINOR 0
#define VERSION_REV   4
#define BETA ""

//#define VERSIONSTR "3.00 beta"
#define VERSIONSTR str_(VERSION_MAJOR) "." str_(VERSION_MINOR) "." str_(VERSION_REV) BETA

#endif
