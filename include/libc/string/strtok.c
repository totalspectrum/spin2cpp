/* from Henry Spencer's stringlib */
#include <string.h>

/*
 * Get next token from string s (NULL on 2nd, 3rd, etc. calls),
 * where tokens are nonempty strings separated by runs of
 * chars from delim.  Writes NULs into s to end tokens.  delim need not
 * remain constant from call to call.
 */

char *				/* NULL if no token left */
strtok(char *s, const char *delim)
{
	register char *scan;
	char *tok;
	register const char *dscan;
        static char *strtok_scanpoint;
        
	if (s == NULL && strtok_scanpoint == NULL)
		return(NULL);
	if (s != NULL)
		scan = s;
	else
		scan = strtok_scanpoint;

	/*
	 * Scan leading delimiters.
	 */
	for (; *scan != '\0'; scan++) {
		for (dscan = delim; *dscan != '\0'; dscan++)
			if (*scan == *dscan)
				break;
		if (*dscan == '\0')
			break;
	}
	if (*scan == '\0') {
		strtok_scanpoint = NULL;
		return(NULL);
	}

	tok = scan;

	/*
	 * Scan token.
	 */
	for (; *scan != '\0'; scan++) {
		for (dscan = delim; *dscan != '\0';)	/* ++ moved down. */
			if (*scan == *dscan++) {
				strtok_scanpoint = scan+1;
				*scan = '\0';
				return(tok);
			}
	}

	/*
	 * Reached end of string.
	 */
	strtok_scanpoint = NULL;
	return(tok);
}
