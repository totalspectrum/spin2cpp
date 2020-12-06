/**
 * @file include/errno.h
 *
 * @brief Defines error condition reporting macros.
 *
 * These simple macros support error conditions for the libraries.
 * The only macros required by ANSI-C are EDOM, EILSEQ, and ERANGE.
 * 
 * The undocumented "Unlikely" and "Network" macros are included
 * for compatibility with GNU libstdc++.
 */
#ifndef _ERRNO_H
#define _ERRNO_H

#ifndef _COMPILER_H
#include <compiler.h>
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#ifdef __FLEXC__
    extern int errno;
#else
#include <sys/thread.h>
#define errno (_TLS->errno)
#endif
    
/**  Undefined error */
#define EOK      0      

/**  Math arg out of domain of func */
#define EDOM     1      

/**  Math result not representable */
#define ERANGE   2      

/**  Illegal sequence */
#define EILSEQ   3      

/**  No such file or directory */
#define ENOENT   4      

/**  Bad file number */
#define EBADF    5      

/**  Permission denied */
#define EACCES   6      

/**  Not enough core */
#define ENOMEM   7      

/**  Temporary failure */
#define EAGAIN   8      

/**  File exists */
#define EEXIST   9      

/**  Invalid argument */
#define EINVAL  10      

/**  Too many open files */
#define EMFILE  11      

/**  I/O error */
#define EIO     12      

/**  Not a directory */
#define ENOTDIR 13      

/**  Is a directory */
#define EISDIR  14      

/**  Read only file system */
#define EROFS   15      

/**  Function not implemented */
#define ENOSYS  16      

/**  Directory not empty */
#define ENOTEMPTY 17    

/**  File or path name too long */
#define ENAMETOOLONG 18 

/**  Device not seekable */
#define ENOSEEK   19    

/**  Bad address */
#define EFAULT    20    

/**  Broken pipe */
#define EPIPE     21    

/**  Device or resource busy */
#define EBUSY     22    

/**  Cross device link */
#define EXDEV     23    

/**  No space on device */
#define ENOSPC    24    

/**  System call interrupted */
#define EINTR     25    

#define ENODEV    26
#define ENFILE    27
#define EDEADLK   28

/* unlikely errors */
/* in fact, these and the networking errors are only here because
   the GNU libstdc++ expects errno.h to define them; the library
   cannot generate them, and strerror will report "unknown error"
   for them */
#define ENOBUFS      50
#define ECHILD       51
#define ENOLCK       52
#define ESRCH        53
#define EMLINK       54
#define ELOOP        55
#define EPROTOTYPE   56
#define ENXIO        57

/* networking errors */
#define EAFNOSUPPORT 100
#define EADDRINUSE   101
#define EADDRNOTAVAIL 102
#define EISCONN      103
#define E2BIG        104
#define ECONNREFUSED 105
#define ECONNRESET   106
#define EDESTADDRREQ 107
#define EHOSTUNREACH 108
#define EMSGSIZE     109
#define ENOMSG       110
#define ENOPROTOOPT  111
#define ENETDOWN     112
#define ENETRESET    113
#define ENETUNREACH  114
#define ENOTSOCK     115
#define EINPROGRESS  116
#define EPROTONOSUPPORT 117
#define ENOTCONN     118
#define ECONNABORTED 119
#define EALREADY     120
#define ETIMEDOUT    121

/* some aliases */

/**  Temporary failure */
#define EWOULDBLOCK  EAGAIN

/**  Broken pipe */
#define ESPIPE       EPIPE

/**  Function not implemented */
#define ENOEXEC      ENOSYS

/**  Math result not representable */
#define EFBIG        ERANGE

/**  Function not implemented */
#define EOPNOTSUPP   ENOSYS

/**  Device not seekable */
#define ENOTTY       ENOSEEK

/**  Permission denied */
#define EPERM        EACCES

    int _seterror(int errnum);
    int _geterror();
    int *_geterrnoptr();

#define errno (*(_geterrnoptr()))

#if defined(__cplusplus)
}
#endif

#endif
