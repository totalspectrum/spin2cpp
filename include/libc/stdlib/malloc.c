#include <stdlib.h>

void *malloc(size_t size) {
    return _gc_alloc(size);
}
void *calloc(size_t n, size_t size) {
    /* _gc_alloc already zeros its returned memory */
    return _gc_alloc(n*size);
}

void free(void *ptr) {
    return _gc_free(ptr);
}
