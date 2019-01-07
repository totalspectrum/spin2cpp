#ifndef _WCTYPE_H
#define _WCTYPE_H

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

  int iswalnum(wint_t wc);
  int iswalpha(wint_t wc);
  int iswblank(wint_t wc);
  int iswcntrl(wint_t wc);
  int iswdigit(wint_t wc);
  int iswgraph(wint_t wc);
  int iswlower(wint_t wc);
  int iswprint(wint_t wc);
  int iswpunct(wint_t wc);
  int iswspace(wint_t wc);
  int iswupper(wint_t wc);
  int iswxdigit(wint_t wc);

  int iswctype(wint_t wc, wctype_t desc);
  wctype_t wctype(const char *property);

  wint_t towlower(wint_t wc);
  wint_t towupper(wint_t wc);
  wint_t towctrans(wint_t wc, wctrans_t desc);
  wctrans_t wctrans(const char *property);

#if defined(__cplusplus)
}
#endif

#endif
