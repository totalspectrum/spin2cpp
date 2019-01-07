/* from Henry Spencer's stringlib */
#include <wchar.h>

/*
 * strlen - length of string (not including NUL)
 */
size_t
wcslen(const wchar_t *scan)
{
	const wchar_t *start = scan+1;

	if (!scan) return 0;
	while (*scan++ != '\0')
		continue;
	return (size_t)(scan - start);
}
