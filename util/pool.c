//
// Pool Allocator
// Copyright 2023 Total Spectrum Software Inc.
// MIT Licensed
//

#include <stdio.h>
#include <string.h>
#include "pool.h"

#undef calloc
#undef malloc
#undef realloc
#undef free
#undef strdup

#define POOL_ELEM_SIZE (1024*1024)
#define POOL_NUM_ELEMS 1024

static void *pool[POOL_NUM_ELEMS];
static void *last_alloc = NULL;
static int cur_pool = -1;
static unsigned cur_pool_size = POOL_ELEM_SIZE;

#define ALIGN(siz) ((siz)+15) & ~15

void *pool_alloc(size_t siz) {
    size_t alloc_siz;
    void *ptr;
    // round up to align on 16 bytes
    siz = ALIGN(siz);
    // see if we need a new pool
    if (siz + cur_pool_size > POOL_ELEM_SIZE) {
        cur_pool++;
        if (cur_pool >= POOL_NUM_ELEMS) {
            fprintf(stderr, "Out of memory pools!\n");
            abort();
        }
        alloc_siz = (siz < POOL_ELEM_SIZE) ? POOL_ELEM_SIZE : siz;
        ptr = pool[cur_pool] = malloc(alloc_siz);
        if (!ptr) {
            fprintf(stderr, "Unable to allocate %lu bytes\n", (unsigned long)alloc_siz);
            abort();
        }
        memset(ptr, 0, alloc_siz);
        cur_pool_size = 0;
    }
    ptr = cur_pool_size + (char *)pool[cur_pool];
    cur_pool_size += siz;
    last_alloc = ptr;
    return ptr;
}

void *pool_calloc(size_t nmeb, size_t siz) {
    return pool_alloc(nmeb * siz);
}

void *pool_realloc(void *ptr, size_t siz) {
    if (!ptr) return pool_alloc(siz);
    siz = ALIGN(siz);

    if (ptr == last_alloc) {
        size_t old_pool_size = ((char *)last_alloc) - (char *)pool[cur_pool];
        size_t old_alloc_size = cur_pool_size - old_pool_size;
        size_t delta = siz - old_alloc_size;
        if (delta <= 0) {
            cur_pool_size += delta;
            return ptr;
        }
        if (cur_pool_size + delta < POOL_ELEM_SIZE) {
            cur_pool_size += delta;
            return ptr;
        }
    }

    void *newptr = pool_alloc(siz);
    if (newptr) {
        memcpy(newptr, ptr, siz);
    }
    return newptr;
}

void pool_free(void *ptr) {    
    if (!ptr) return;
#if 1
    if (ptr == last_alloc) {
        size_t old_pool_size = ((char *)last_alloc) - (char *)pool[cur_pool];
        ssize_t delta = cur_pool_size - old_pool_size;
        if (delta <= 0) {
            abort();
        }
        cur_pool_size = old_pool_size;
        memset(ptr, 0, delta);
        last_alloc = NULL;
    }
#endif    
}

void pool_reinit(void) {
    int i;

    for (i = 0; i <= cur_pool; i++) {
        if (pool[i]) {
            free(pool[i]);
            pool[i] = 0;
        }
    }
    cur_pool = -1;
    last_alloc = NULL;
    cur_pool_size = POOL_ELEM_SIZE;
}

char *pool_strdup(const char *src) {
    char *dst;
    if (!src) return NULL;
    size_t n = strlen(src) + 1;
    dst = pool_alloc(n);

    memcpy(dst, src, n);
    return dst;
}
