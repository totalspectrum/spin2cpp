#ifndef _STRING_H
#define _STRING_H

#include <sys/size_t.h>
#include <sys/null.h>
#include <compiler.h>

#if defined(__cplusplus)
extern "C" {
#endif

    void * memcpy(void *dest, const void *src, size_t n) _IMPL("libc/string/memcpy.c");
    void * memmove(void *dest, const void *src, size_t n) _IMPL("libc/string/memmove.c");

    char * strcpy(char * __restrict dest, const char * __restrict src) _IMPL("libc/string/strcpy.c");
    char * strncpy(char * __restrict dest, const char * __restrict src, size_t n) _IMPL("libc/string/strncpy.c");
    char * strcat(char * __restrict dest, const char * __restrict src) _IMPL("libc/string/strcat.c");
    char * strncat(char * __restrict dest, const char * __restrict src, size_t n) _IMPL("libc/string/strncat.c");

    size_t strcspn(char *s, const char *accept) _IMPL("libc/string/strcspn.c");
    size_t strspn(char *s, const char *reject) _IMPL("libc/string/strspn.c");
    
    int    memcmp(const void *s1, const void *s2, size_t n) _IMPL("libc/string/memcmp.c");
    int    strcmp(const char *s1, const char *s2) _IMPL("libc/string/strcmp.c");
    int    strcoll(const char *s1, const char *s2);
    int    strncmp(const char *s1, const char *s2, size_t n) _IMPL("libc/string/strncmp.c");
    size_t strxfrm(char *dest, const char *src, size_t n);
    
    char * strdup (const char *s1) _IMPL("libc/string/strdup.c");

    void * memchr(const void *s, int c, size_t n) _IMPL("libc/string/memchr.c");
    char * strchr(const char *, int) _IMPL("libc/string/strchr.c");
    size_t strcspn(const char *s1, const char *s2);
    char * strpbrk(const char *str, const char *accept);
    char * strrchr(const char *, int) _IMPL("libc/string/strrchr.c");
    size_t strspn(const char *, const char *);
    char * strstr(const char *src, const char *pattern) _IMPL("libc/string/strstr.c");
    char * strtok(char * __restrict str, const char * __restrict delim) _IMPL("libc/string/strtok.c");


    void * memset(void *dest, int c, size_t n) _IMPL("libc/string/memset.c");
    char * strerror(int err) _IMPL("libc/string/strerror.c");
    size_t strlen(const char *s) _IMPL("libc/string/strlen.c");

    extern int    _strcasecmp(const char *s1, const char *s2);
    extern int    _strncasecmp(const char *s1, const char *s2, size_t n);

    char   *_strrev(char *str);  /* reverse string in place */
    char   *_strupr(char *str);  /* convert to upper case in place */

#if !defined(__STRICT_ANSI__)
    int    strcasecmp(const char *s1, const char *s2) _IMPL("libc/string/strcasecmp.c");
    int    strncasecmp(const char *s1, const char *s2, size_t n) _IMPL("libc/string/strncasecmp.c");
#endif

#if defined(__cplusplus)
}
#endif

#ifdef __FLEXC__
#define strcpy(a, b) __builtin_strcpy(a, b)
#define strlen(a)    __builtin_strlen(a)
#define memset(a, v, n) __builtin_memset(a, v, n)
#define memcpy(a, b, n) __builtin_memcpy(a, b, n)
#define memmove(a, b, n) __builtin_memmove(a, b, n)
#endif

#endif
