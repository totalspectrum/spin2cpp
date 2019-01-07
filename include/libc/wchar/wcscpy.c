/* from Henry Spencer's stringlib */

#include <wchar.h>
#undef strcpy

/*
 * strcpy - copy string src to dst
 */
wchar_t *				/* dst */
wcscpy(wchar_t * __restrict dst, const wchar_t * __restrict src)
{
	register wchar_t *dscan = dst;
	register const wchar_t *sscan = src;

	if (!sscan) return(dst);
	while ((*dscan++ = *sscan++) != L'\0')
		continue;
	return(dst);
}
