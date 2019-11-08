#ifndef _STDDEF_H
#define _STDDEF_H

#include <sys/size_t.h>
#include <sys/null.h>
#include <sys/wchar_t.h>

typedef long ptrdiff_t;

#define offsetof(T, f) ( (size_t) & (((T *)0)->f) )

#endif
