enum {
    DEF1 = 2,
    DEF2 = 8
};

typedef struct pt {
    int a;
    int x[DEF2];
} PT;

void report(PT *p, int i)
{
    _OUTA = p->x[i] + sizeof(*p);
}

