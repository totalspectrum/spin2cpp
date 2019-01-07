/* from Henry Spencer's stringlib */

#include <wchar.h>

/*
 * wcsncpy - copy at most n characters of string src to dst
 */
wchar_t *				/* dst */
wcsncpy(wchar_t * __restrict dst, const wchar_t * __restrict src, size_t n)
{
	register wchar_t *dscan;
	register const wchar_t *sscan;
	register long count;

	dscan = dst;
	sscan = src;
	count = n;
	while (--count >= 0 && (*dscan++ = *sscan++) != L'\0')
		continue;
	while (--count >= 0)
		*dscan++ = L'\0';
	return(dst);
}
