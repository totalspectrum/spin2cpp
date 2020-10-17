#ifndef _WCHAR_H
#define _WCHAR_H

#define _NEED_WINT_T
#include <sys/wchar_t.h>
#include <sys/size_t.h>
#include <sys/null.h>

#ifndef WEOF
#define WEOF ((wint_t)-1)
#endif

#if defined(__cplusplus)
extern "C" {
#endif
  typedef _Mbstate_t mbstate_t;

  int    mbsinit(const mbstate_t *ps);
  size_t mbrlen(const char *__restrict s, size_t n, mbstate_t *__restrict ps);
  size_t mbrtowc( wchar_t *pwc, const char *s, size_t n, mbstate_t *ps);
  size_t wcrtomb( char *s, wchar_t wc, mbstate_t *ps );
  size_t mbsrtowcs( wchar_t *dst, const char **src, size_t n, mbstate_t *ps);

  wint_t btowc(int c);
  int    wctob(wint_t c);

  long int wcstol( const wchar_t *__restrict nptr,
		   wchar_t **__restrict endptr, int base);
  unsigned long wcstoul( const wchar_t *__restrict nptr,
			     wchar_t **__restrict endptr, int base);
  long long wcstoll( const wchar_t *__restrict nptr,
		   wchar_t **__restrict endptr, int base);
  unsigned long long wcstoull( const wchar_t *__restrict nptr,
			     wchar_t **__restrict endptr, int base);

  wchar_t *wcscpy(wchar_t *__restrict s1, const wchar_t *__restrict s2);
  wchar_t *wcsncpy(wchar_t *__restrict s1, const wchar_t *__restrict s2, size_t n);
  wchar_t *wmemcpy(wchar_t *__restrict s1, const wchar_t *__restrict s2, size_t n);
  wchar_t *wmemmove(wchar_t *s1, const wchar_t *s2, size_t n);

  wchar_t *wcscat(wchar_t *__restrict s1, const wchar_t *__restrict s2);
  wchar_t *wcsncat(wchar_t *__restrict s1, const wchar_t *__restrict s2, size_t n);

  int wcscmp(const wchar_t *s1, const wchar_t *s2);
  int wcscoll(const wchar_t *s1, const wchar_t *s2);
  int wcsncmp(const wchar_t *s1, const wchar_t *s2, size_t n);
  size_t wcsxfrm(wchar_t *__restrict s1, const wchar_t *__restrict s2, size_t n);
  int wmemcmp(const wchar_t *s1, const wchar_t *s2, size_t n);

  wchar_t *wcschr(const wchar_t *s, wchar_t c);
  wchar_t *wcsrchr(const wchar_t *s, wchar_t c);
  size_t   wcslen(const wchar_t *s);


  /* internal pointers used for conversion */
  extern size_t (*_mbrtowc_ptr)( wchar_t *pwc, const char *s, size_t n, mbstate_t *ps);
  extern size_t (*_wcrtomb_ptr)( char *s, wchar_t wc, mbstate_t *ps );

  extern size_t _mbrtowc_utf8( wchar_t *pwc, const char *s, size_t n, mbstate_t *ps);
  extern size_t _mbrtowc_ascii( wchar_t *pwc, const char *s, size_t n, mbstate_t *ps);

  extern size_t _wcrtomb_utf8( char *s, wchar_t wc, mbstate_t *ps );
  extern size_t _wcrtomb_ascii( char *s, wchar_t wc, mbstate_t *ps );

#if defined(__cplusplus)
}
#endif


#endif
