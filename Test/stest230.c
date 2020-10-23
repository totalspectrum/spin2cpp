typedef struct point {
    int x, y, z;
} PT;

void show(PT a) {
    _OUTA = a.x;
    _OUTB = a.y;
    _INA = a.z;
}

int main()
{
    show( ({ PT s_ = {1, 2, 3}; s_; }) );
    return 0;
}
