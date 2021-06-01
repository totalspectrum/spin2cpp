#include "version.h"

#ifndef BETA
#define BETA
#ifndef GITBRANCH
#undef GITREV
#endif
#endif

#ifndef GITREV
#define GITREVSEP
#define GITREV
#else
#define GITREVSEP "-"
#endif

#ifndef GITBRANCH
#define GITRRANCHSEP
#define GITBRANCH
#else
#define GITRRANCHSEP "-"
#endif

char version_string[] = str_(VERSION_MAJOR) "." str_(VERSION_MINOR) "." str_(VERSION_REV) BETA GITRRANCHSEP str_(GITBRANCH) GITREVSEP str_(GITREV);
