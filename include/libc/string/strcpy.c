/* from Henry Spencer's stringlib */

#include <string.h>
#include <compiler.h>
#undef strcpy

/*
 * strcpy - copy string src to dst
 */
_CACHED
char *				/* dst */
strcpy(char * __restrict dst, const char * __restrict src)
{
	register char *dscan = dst;
	register const char *sscan = src;

	if (!sscan) return(dst);
	while ((*dscan++ = *sscan++) != '\0')
		continue;
	return(dst);
}
