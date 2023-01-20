#ifndef POOL_ALLOC_H
#define POOL_ALLOC_H

#pragma once

#include <stdlib.h>

void pool_reinit(void);
void *pool_alloc(size_t siz);
void *pool_calloc(size_t nmeb, size_t siz);
void *pool_realloc(void *ptr, size_t siz);
void pool_free(void *ptr);
char *pool_strdup(const char *src);

#define malloc pool_alloc
#define calloc pool_calloc
#define realloc pool_realloc
#define free   pool_free
#define strdup pool_strdup

#endif
