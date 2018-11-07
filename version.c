#include "version.h"

#ifndef BETA
#define BETA
#undef GITREV
#endif

#ifndef GITREV
#define GITREVSEP
#define GITREV
#else
#define GITREVSEP "-"
#endif

char version_string[] = str_(VERSION_MAJOR) "." str_(VERSION_MINOR) "." str_(VERSION_REV) BETA GITREVSEP str_(GITREV);
