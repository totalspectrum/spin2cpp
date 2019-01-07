/* from Henry Spencer's stringlib */
/* modified by ERS */

#include <wchar.h>

/*
 * wcscmp - compare string s1 to s2
 */

int				/* <0 for <, 0 for ==, >0 for > */
wcscmp(const wchar_t *scan1, const wchar_t *scan2)
{
	register wchar_t c1, c2;

	if (!scan1)
		return scan2 ? -1 : 0;
	if (!scan2) return 1;

	do {
		c1 = *scan1++; c2 = *scan2++;
	} while (c1 && c1 == c2);

	/*
	 * The following case analysis is necessary so that characters
	 * which look negative collate low against normal characters but
	 * high against the end-of-string NUL.
	 */
	if (c1 == c2)
		return(0);
	else if (c1 == L'\0')
		return(-1);
	else if (c2 == L'\0')
		return(1);
	else
		return(c1 - c2);
}
