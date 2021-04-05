typedef struct point {
    short x;
    short y;
} Point;

void plot(Point const *p)
{
    outa = p->x;
    outb = p->y;
}
