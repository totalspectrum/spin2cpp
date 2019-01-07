/* from Henry Spencer's stringlib */
/* check for src==NULL and wide character modification added by ERS */

#include <wchar.h>
#undef strcat

/*
 * wcscat - append string src to dst
 */
wchar_t *				/* dst */
wcscat(wchar_t *dst, const wchar_t *src)
{
	register wchar_t *dscan;
	register const wchar_t *sscan;

	if ((sscan = src) != NULL)
	{
	    for (dscan = dst; *dscan != '\0'; dscan++)
		continue;
	    while ((*dscan++ = *sscan++) != '\0')
			continue;
	}
	return(dst);
}
