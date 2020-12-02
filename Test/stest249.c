#include <propeller2.h>

typedef struct Position {
    int x, y;
} Position;

Position insertstr(Position p, int s) __attribute__(noinline)
{
    p.x += s;
    return p;
}

void main()
{
    Position p = {1, 2};
    Position q;
    q = insertstr(p, 99);
    _OUTA = p.x;
    _OUTB = q.x;
}
