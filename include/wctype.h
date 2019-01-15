#ifndef _WCTYPE_H
#define _WCTYPE_H

#include <compiler.h>

#define _NEED_WINT_T
#include <sys/wchar_t.h>

#ifndef WEOF
#define WEOF ((wint_t)-1)
#endif

typedef unsigned int wctrans_t;

#if defined(__cplusplus)
extern "C" {
#endif

#include <sys/ctype.h>

  typedef _CTYPE_T wctype_t;

    int iswalnum(wint_t wc) _IMPL("libc/wchar/iswalnum.c");
    int iswalpha(wint_t wc) _IMPL("libc/wchar/iswalpha.c");
    int iswblank(wint_t wc) _IMPL("libc/wchar/iswblank.c");
    int iswcntrl(wint_t wc) _IMPL("libc/wchar/iswcntrl.c");
    int iswdigit(wint_t wc) _IMPL("libc/wchar/iswdigit.c");
    int iswgraph(wint_t wc) _IMPL("libc/wchar/iswgraph.c");
    int iswlower(wint_t wc) _IMPL("libc/wchar/iswlower.c");
    int iswprint(wint_t wc) _IMPL("libc/wchar/iswprint.c");
    int iswpunct(wint_t wc) _IMPL("libc/wchar/iswpunct.c");
    int iswspace(wint_t wc) _IMPL("libc/wchar/iswspace.c");
    int iswupper(wint_t wc) _IMPL("libc/wchar/iswupper.c");
    int iswxdigit(wint_t wc) _IMPL("libc/wchar/iswxdigit.c");

    int iswctype(wint_t wc, wctype_t desc) _IMPL("libc/wchar/iswctype.c");
    wctype_t wctype(const char *property) _IMPL("libc/wchar/wctype.c");

    wint_t towlower(wint_t wc) _IMPL("libc/wchar/towlower.c");
    wint_t towupper(wint_t wc) _IMPL("libc/wchar/towupper.c");
    wint_t towctrans(wint_t wc, wctrans_t desc);
    wctrans_t wctrans(const char *property);

#if defined(__cplusplus)
}
#endif

#endif
