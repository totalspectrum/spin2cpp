/* from Henry Spencer's stringlib */
#include <wchar.h>

/*
 * strrchr - find last occurrence of a character in a string
 */

wchar_t *				/* found char, or NULL if none */
wcsrchr(const wchar_t *s, int charwanted)
{
	register wchar_t c;
	register const wchar_t *place;

	place = NULL;
	while ((c = *s++) != 0)
		if (c == (wchar_t) charwanted)
			place = s - 1;
	if ((wchar_t) charwanted == '\0')
		return((wchar_t *)--s);
	return (wchar_t *)place;
}
