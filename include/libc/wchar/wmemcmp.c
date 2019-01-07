/* original from Henry Spencer's stringlib */

#include <stddef.h>
#include <string.h>
#include <wchar.h>

/*
 * wmemcmp - compare wide characters
 */

#define	UNSCHAR(c)	((unsigned int)(c))

int				/* <0, == 0, >0 */
wmemcmp(const wchar_t *scan1, const wchar_t *scan2, size_t size)
{
	register size_t n;

	for (n = size; n > 0; n--)
		if (*scan1 == *scan2) {
			scan1++;
			scan2++;
		} else
			return(UNSCHAR (*scan1) - UNSCHAR (*scan2));

	return(0);
}
