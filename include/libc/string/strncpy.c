/* from Henry Spencer's stringlib */

#include <string.h>
#include <compiler.h>

/*
 * strncpy - copy at most n characters of string src to dst
 */
_CACHED
char *				/* dst */
strncpy(char * __restrict dst, const char * __restrict src, size_t n)
{
	register char *dscan;
	register const char *sscan;
	register long count;

	dscan = dst;
	sscan = src;
	count = n;
	while (--count >= 0 && (*dscan++ = *sscan++) != '\0')
		continue;
	while (--count >= 0)
		*dscan++ = '\0';
	return(dst);
}
