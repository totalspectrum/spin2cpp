struct blah {
    int x;
    struct lfs_ctz {
        int head;
        int size;
    } ctz;
} y;

// Count the number of trailing binary zeros in a
// lfs_ctz(0) may be undefined
static inline unsigned lfs_ctz(unsigned a) {
    return a & 0xff;
}

void lfs_trunc(struct lfs_ctz *ctz) {
    ctz->head = lfs_ctz(ctz->head) + y.ctz.head;
    ctz->size = (ctz->size) & 0x1;
}

