/* from Henry Spencer's stringlib */
#include <wchar.h>

/*
 * wcsncat - append at most n characters of string src to dst
 */
wchar_t *				/* dst */
wcsncat(dst, src, n)
wchar_t *dst;
const wchar_t *src;
size_t n;
{
	register wchar_t *dscan, c;
	register const wchar_t *sscan;
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
