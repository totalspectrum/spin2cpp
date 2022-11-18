#ifndef _TERMIOS_H
#define _TERMIOS_H

#include <compiler.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define ICRNL  0x0001

#define ECHO   0x0100
#define ICANON 0x0200

#define VEOF    0
#define VEOL    1
#define VERASE  2
#define VINTR   3
#define VKILL   4
#define VQUIT   5

#define NCCS 8

typedef unsigned int  tcflag_t;
typedef unsigned char cc_t;

struct termios {
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;
    cc_t     c_cc[NCCS];
};

    int tcgetattr(int fd, struct termios *termios_p) _IMPL("libc/unix/termios.c");
    int tcsetattr(int fd, struct termios *termios_p) _IMPL("libc/unix/termios.c");
    
#if defined(__cplusplus)
}
#endif

#endif
