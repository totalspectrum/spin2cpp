#include <termios.h>

/* FIXME: these do nothing useful yet */
int tcgetattr(int fd, struct termios *termios_p) {
    memset(termios_p, 0, sizeof(struct termios));
    return 0;
}

int tcsetattr(int fd, struct termios *termios_p) {
    return 0;
}
