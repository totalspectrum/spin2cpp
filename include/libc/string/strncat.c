/* from Henry Spencer's stringlib */
#include <string.h>

/*
 * strncat - append at most n characters of string src to dst
 */
char *				/* dst */
strncat(dst, src, n)
char *dst;
const char *src;
size_t n;
{
	register char *dscan, c;
	register const char *sscan;
	register long count;

	if(((sscan = src) != NULL) && (n > 0))
	{
	    for (dscan = dst; *dscan != '\0'; dscan++)
		continue;
	    count = n;
	    while ((c = *sscan++) != '\0' && --count >= 0)
		*dscan++ = c;
	    *dscan = '\0';
	}
	return(dst);
}
