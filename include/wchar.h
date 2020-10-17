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

    int    mbsinit(const mbstate_t *ps) _IMPL("libc/wchar/mbsinit.c");
    size_t mbrlen(const char *__restrict s, size_t n, mbstate_t *__restrict ps) _IMPL("libc/wchar/mbrlen.c");
    size_t mbrtowc( wchar_t *pwc, const char *s, size_t n, mbstate_t *ps) _IMPL("libc/wchar/mbrtowc_utf.c");
    size_t wcrtomb( char *s, wchar_t wc, mbstate_t *ps ) _IMPL("libc/wchar/wcrtomb_utf.c");
    size_t mbsrtowcs( wchar_t *dst, const char **src, size_t n, mbstate_t *ps) _IMPL("libc/wchar/mbsrtowcs.c");

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

    wchar_t *wcscpy(wchar_t *__restrict s1, const wchar_t *__restrict s2) _IMPL("libc/wchar/wcscpy.c");
    wchar_t *wcsncpy(wchar_t *__restrict s1, const wchar_t *__restrict s2, size_t n) _IMPL("libc/wchar/wcsncpy.c");
    wchar_t *wmemcpy(wchar_t *__restrict s1, const wchar_t *__restrict s2, size_t n) _IMPL("libc/wchar/wmemcpy.c");
    wchar_t *wmemmove(wchar_t *s1, const wchar_t *s2, size_t n) _IMPL("libc/wchar/wmemmove.c");

    wchar_t *wcscat(wchar_t *__restrict s1, const wchar_t *__restrict s2) _IMPL("libc/wchar/wcscat.c");
    wchar_t *wcsncat(wchar_t *__restrict s1, const wchar_t *__restrict s2, size_t n) _IMPL("libc/wchar/wcsncat.c");

    int wcscmp(const wchar_t *s1, const wchar_t *s2) _IMPL("libc/wchar/wcscmp.c");
    int wcscoll(const wchar_t *s1, const wchar_t *s2) _IMPL("libc/wchar/wcscoll.c");
    int wcsncmp(const wchar_t *s1, const wchar_t *s2, size_t n) _IMPL("libc/wchar/wcsncmp.c");
    size_t wcsxfrm(wchar_t *__restrict s1, const wchar_t *__restrict s2, size_t n) _IMPL("libc/wchar/wcsxfrm.c");
    int wmemcmp(const wchar_t *s1, const wchar_t *s2, size_t n) _IMPL("libc/wchar/wmemcmp.c");

    wchar_t *wcschr(const wchar_t *s, wchar_t c) _IMPL("libc/wchar/wcschr.c");
    wchar_t *wcsrchr(const wchar_t *s, wchar_t c) _IMPL("libc/wchar/wcsrchr.c");
    size_t   wcslen(const wchar_t *s) _IMPL("libc/wchar/wcslen.c");

#ifndef _FLEXC
    /* internal pointers used for conversion */
    extern size_t (*_mbrtowc_ptr)( wchar_t *pwc, const char *s, size_t n, mbstate_t *ps);
    extern size_t (*_wcrtomb_ptr)( char *s, wchar_t wc, mbstate_t *ps );

    extern size_t _mbrtowc_utf8( wchar_t *pwc, const char *s, size_t n, mbstate_t *ps);
    extern size_t _mbrtowc_ascii( wchar_t *pwc, const char *s, size_t n, mbstate_t *ps);

    extern size_t _wcrtomb_utf8( char *s, wchar_t wc, mbstate_t *ps );
    extern size_t _wcrtomb_ascii( char *s, wchar_t wc, mbstate_t *ps );
#endif
    
#if defined(__cplusplus)
}
#endif


#endif
