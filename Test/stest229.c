typedef struct point {
    int x, y, z;
} PT;

void show(PT a) {
    _OUTA = a.y;
    _OUTB = a.z;
}

