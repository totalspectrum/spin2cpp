typedef struct foo {
    char x;
    char y;
    int z;
} Foo;

int getsiz(Foo *ptr)
{
    return sizeof(*ptr);
}

int getx(Foo *ptr)
{
    return ptr[0].x;
}
int gety(struct foo *ptr)
{
    return ptr->y;
}
int getz(Foo *ptr)
{
    return ptr->z;
}
