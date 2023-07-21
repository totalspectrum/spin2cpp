/*
 * Written by Eric R. Smith (ersmith@totalspectrum.ca)
 * and placed in the public domain
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <compiler.h>
#include <propeller.h>

/*
 * mktemp: replace trailing 6 XXXXXX with a unique value
 */

/* we use the name "_mktemp", which is in the ANSI reserved namespace,
 * so that the function can be called from standard C libraries;
 * a weak alias to "mktemp" is provided so that users can call it
 * directly if they want
 */
char *_mktemp(char *template)
{
  static int tmpnum;
  int numxs = 0;
  char *s;
  int xx, digit;
  int i;
  /* first, verify that the last 6 characters are XXXXXX */
  for (s = template; s && *s; s++) {
    if (*s == 'X')
      numxs++;
    else
      numxs = 0;
  }
  if (numxs < 6) {
    errno = EINVAL;
    *template = 0;
    return template;
  }
  /* right now "s" points at the trailing 0
     put it back 6 spaces to make room */
  s -= 6;

  /* atomically increment the temporary number */
  /* FIXME: this needs to actually be atomic! */
  xx = tmpnum++;

  /* create the template */
  for (i = 0; i < 6; i++) {
    digit = (xx & 0xF) + 'A';
    s[i] = digit;
    xx >>= 4;
  }
  return template;
}

#ifdef __weak_alias
__weak_alias(mktemp, _mktemp);
#else
char *mktemp(char *tmp)
{
    return _mktemp(tmp);
}
#endif
