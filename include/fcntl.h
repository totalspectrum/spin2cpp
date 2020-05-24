/**
 * @file include/fcntl.h
 *
 * @brief Provides fcntl() interface API macros.
 *
 * The fcntl() function is only provided in this library to support libstdc++ std namespace.
 *
 * The fcntl() function is not really useful as Propeller-GCC does not implement fcntl().
 *
 */
#ifndef _FCNTL_H
#define _FCNTL_H

/** Set open file status to read only */
#define O_RDONLY 0

/** Set open file status to write only */
#define O_WRONLY 1

/** Set open file status to read-write */
#define O_RDWR   2

/** Mask for access modes */
#define O_ACCMODE (O_RDWR|O_WRONLY)

/** Set open file mode to create */
#define O_CREAT  4

/** Set open file mode to truncate */
#define O_TRUNC  8

/** Set open file mode to exclusive */
#define O_EXCL   16

/** Set open file to append */
#define O_APPEND 32

/** 
 * Definition provided for convenience and libstdc++ build only.
 * Propeller-GCC does not allow forking applications.
 *
 * If bit is 0, the file descriptor will remain open across
 * execve(2), otherwise it will be closed.
 */
#define FD_CLOEXEC 0x10000

/**
 * Definition provided for convenience and libstdc++ build only.
 * Propeller-GCC does not implement fcntl().
 *
 * Set the file descriptor flags to the value specified by arg.
 */
#define F_SETFD    0x20000

#ifdef __FLEXC__
int open(const char *name, int flags, mode_t mode=0644) _IMPL("libc/unix/posixio.c");
#else
/**
 * Open a file; this translates into a call to fopen.
 * Note that open may be declared with either 2 or 3 arguments,
 * and many legacy applications do not declare it with the modern
 * ... notation for variable number of arguments, so we deliberately
 * make its declaration old-style and without prototypes.
 */
extern int open();
#endif

int creat(const char *name, mode_t mode) _IMPL("libc/unix/posixio.c");

#endif
