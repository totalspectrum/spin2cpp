#ifndef _STDLIB_H
#define _STDLIB_H

#include <compiler.h>
#include <sys/size_t.h>
#include <sys/wchar_t.h>
#include <sys/null.h>

#ifdef __FLEXC__
# ifndef abs
#define abs(x) __builtin_abs(x)
# endif
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0

    double atof(const char *) _IMPL("libc/stdlib/atof.c");
    int    atoi(const char *) _IMPL("libc/stdlib/atoi.c");
    long   atol(const char *) _IMPL("libc/stdlib/atoi.c");
    long long atoll(const char *);

    long double strtold(const char *nptr, char **endptr);
#ifdef __FLEXC__
    /* for now only 32 bit doubles are supported in FlexC */
# define strtod(nptr, endptr) strtof(nptr, endptr)
#else
    double strtod(const char *nptr, char **endptr) _IMPL("libc/stdlib/strtod.c");
#endif
    float  strtof(const char *nptr, char **endptr) _IMPL("libc/stdlib/strtof.c");

    long strtol(const char *nptr, char **endptr, int base) _IMPL("libc/stdlib/strtol.c");
    unsigned long strtoul(const char *nptr, char **endptr, int base) _IMPL("libc/stdlib/strtoul.c");
    long long strtoll(const char *nptr, char **endptr, int base);
    unsigned long long strtoull(const char *nptr, char **endptr, int base);

#define RAND_MAX    0x3fffffff
    int rand(void) _IMPL("libc/stdlib/rand.c");
    void srand(unsigned int seed) _IMPL("libc/stdlib/rand.c");

    void *malloc(size_t n) _IMPL("libc/stdlib/malloc.c");
    void *calloc(size_t, size_t) _IMPL("libc/stdlib/malloc.c");
    void *realloc(void *, size_t) _IMPL("libc/stdlib/malloc.c");
    void free(void *) _IMPL("libc/stdlib/malloc.c");

#define ATEXIT_MAX (32)
    int atexit(void (*func)(void)) _IMPL("libc/stdlib/exit.c");
    _NORETURN void exit(int status) _IMPL("libc/stdlib/exit.c");
    _NORETURN void abort(void) _IMPL("libc/stdlib/abort.c");
    _NORETURN void _Exit(int status) _IMPL("libc/stdlib/_Exit.c");  /* like exit(), but skips atexit() stuff */
  _NORETURN void _exit(int status);  /* nonstandard name for _Exit() */

#ifndef abs
  _CONST int abs(int i);
#endif
#ifndef labs    
  _CONST long labs(long l);
#endif
#ifndef llabs
  _CONST long long llabs(long long ll);
#endif
    
  typedef struct {
    int quot, rem;
  } div_t;

  typedef struct {
    long int quot, rem;
  } ldiv_t;

  typedef struct {
    long long quot, rem;
  } lldiv_t;

  div_t div(int num, int denom);
  ldiv_t ldiv(long num, long denom);
  lldiv_t lldiv(long long num, long long denom);

    void qsort(void *base, size_t nmemb, size_t size, int (*compare)(const void *, const void *)) _IMPL("libc/stdlib/qsort.c");
  void *bsearch(const void *key, const void *base, size_t nmemb, size_t size,
		int (*compare)(const void *, const void *));

    char *getenv(const char *name) _IMPL("libc/stdlib/getenv.c");
    int putenv(const char *name) _IMPL("libc/stdlib/putenv.c");

  /* multibyte character functions */
  extern int _mb_cur_max;
#define MB_CUR_MAX _mb_cur_max
#define MB_LEN_MAX 4  /* in Unicode up to 4 UTF-8 bytes per unicode wchar_t */

  int mblen(const char *s, size_t n);
  int mbtowc(wchar_t * __restrict pwc, const char * __restrict s, size_t n);
  size_t mbstowcs(wchar_t *dest, const char *src, size_t n);

  int system(const char *command) _IMPL("libc/stdlib/system.c");

#if defined(_POSIX_SOURCE) || defined(_GNU_SOURCE)
  /* some non-ANSI functions */
  int putenv(char *string);
#endif

    int _itoa_prec( unsigned int x, char *buf, unsigned base, int prec );
    int _lltoa_prec( unsigned long long x, char *buf, unsigned base, int prec );
        
#if defined(__cplusplus)
}
#endif

#endif
