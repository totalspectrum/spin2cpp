#ifndef SIGNAL_H
#define SIGNAL_H

#include <compiler.h>
#include <sys/thread.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef void (*sighandler_t)(int);
typedef _atomic_t sig_atomic_t;

#define SIG_DFL ((sighandler_t)0)
#define SIG_IGN ((sighandler_t)1)
#define SIG_ERR ((sighandler_t)-1)

#define SIGNULL   0
#define SIGHUP    1
#define SIGINT    2
#define SIGQUIT   3
#define SIGILL    4
#define SIGTRAP   5
#define SIGABRT   6
#define SIGPRIV   7
#define SIGFPE    8
#define SIGKILL   9
#define SIGBUS    10
#define SIGSEGV   11
#define SIGSYS    12
#define SIGPIPE   13
#define SIGALRM   14
#define SIGTERM   15

#define _NSIGS 16

    sighandler_t signal(int sig, sighandler_t handler) _IMPL("libc/stdlib/signal.c");
    int raise(int sig) _IMPL("libc/stdlib/signal.c");

#if defined(__cplusplus)
}
#endif

#endif
