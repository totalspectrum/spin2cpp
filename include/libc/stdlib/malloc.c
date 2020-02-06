#include <stdlib.h>
#include <string.h>

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

void *realloc(void *ptr, size_t size)
{
    void *newptr;
    if (ptr == 0) {
        return malloc(size);
    }
    if (size == 0) {
        free(ptr);
        return 0;
    }
    newptr = malloc(size);
    if (newptr) {
        memcpy(newptr, ptr, size);
        free(ptr);
    }
    return newptr;
}
