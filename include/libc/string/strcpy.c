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
        int c;
        
	if (!sscan) return(dst);
        do {
            c = *sscan++;
            *dscan++ = c;
        } while (c != 0);
	return(dst);
}
