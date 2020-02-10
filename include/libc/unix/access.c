/* access() emulation; relies heavily on stat() */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

int
access(
    const char *path,
    int mode)
{
	struct stat sb;

	if (stat(path, &sb) < 0)
		return -1;	/* errno was set by stat() */
	if (mode == F_OK)
		return 0;	/* existence test succeeded */

/* somewhat crufty code -- relies on R_OK, etc. matching the bits in the
   file mode, but what the heck, we can do this
 */
	if ( ( getuid() == sb.st_uid ) ) {
		if ( ((sb.st_mode >> 6) & mode) == mode )
			return 0;
		else
			goto accdn;
	}

	if ( getgid() == sb.st_gid ) {
		if ( ((sb.st_mode >> 3) & mode) == mode )
			return 0;
		else
			goto accdn;
	}

	if ( (sb.st_mode & mode) == mode)
		return 0;
accdn:
	errno = EACCES; return -1;
}
