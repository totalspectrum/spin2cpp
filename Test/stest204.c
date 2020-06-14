#include <propeller.h>

#define SOMECONST 64.0 / 2.0
#define SOMECONST2 65536.0 + 1

void main() {
    _DIRA = SOMECONST;
    _OUTA = SOMECONST2;
}
