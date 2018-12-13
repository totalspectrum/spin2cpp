#ifndef _STRING_H
#define _STRING_H

#include <sys/size_t.h>
#include <sys/null.h>

#if defined(__cplusplus)
extern "C" {
#endif

void * memcpy(void *dest, const void *src, size_t n);
void * memmove(void *dest, const void *src, size_t n);

char * strcpy(char * __restrict dest, const char * __restrict src);
char * strncpy(char * __restrict dest, const char * __restrict src, size_t n);
char * strcat(char * __restrict dest, const char * __restrict src);
char * strncat(char * __restrict dest, const char * __restrict src, size_t n);

int    memcmp(const void *s1, const void *s2, size_t n);
int    strcmp(const char *s1, const char *s2);
int    strcoll(const char *s1, const char *s2);
int    strncmp(const char *s1, const char *s2, size_t n);
size_t strxfrm(char *dest, const char *src, size_t n);

char * strdup (const char *s1);

void * memchr(const void *s, int c, size_t n);
char * strchr(const char *, int);
size_t strcspn(const char *s1, const char *s2);
char * strpbrk(const char *str, const char *accept);
char * strrchr(const char *, int);
size_t strspn(const char *, const char *);
char * strstr(const char *src, const char *pattern);
char * strtok(char * __restrict str, const char * __restrict delim);


void * memset(void *dest, int c, size_t n);
char * strerror(int err);
size_t strlen(const char *s);

  extern int    _strcasecmp(const char *s1, const char *s2);
  extern int    _strncasecmp(const char *s1, const char *s2, size_t n);

  char   *_strrev(char *str);  /* reverse string in place */
  char   *_strupr(char *str);  /* convert to upper case in place */

#if !defined(__STRICT_ANSI__)
  int    strcasecmp(const char *s1, const char *s2);
  int    strncasecmp(const char *s1, const char *s2, size_t n);
#endif

#if defined(__cplusplus)
}
#endif

#endif
