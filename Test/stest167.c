typedef struct list {
    int data;
    struct list *next;
    struct list *x;
} LIST;

LIST *stepif(LIST *e)
{
    if (e->data) {
        e = e->next;
    }
    return e;
}
LIST *storeif(LIST *e)
{
    if (e->data) {
        e->x = e;
    }
    return e;
}
