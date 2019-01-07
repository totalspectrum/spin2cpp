/* from Henry Spencer's stringlib */
#include <wchar.h>

/*
 * strchr - find first occurrence of a character in a string
 */

wchar_t *				/* found char, or NULL if none */
wcschr(const wchar_t *s, int charwanted)
{
	register wchar_t c;

	/*
	 * The odd placement of the two tests is so NUL is findable.
	 */
	while ((c = *s++) != (wchar_t) charwanted)
		if (c == 0) return NULL;
	return((wchar_t *)--s);
}
