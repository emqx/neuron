#include <neuron.h>

#include "neu_mem_cache.h"

typedef struct element {
    struct element *prev;
    struct element *next;
} element;

struct neu_mem_cache {
    size_t   max_bytes;
    size_t   max_msgs;
    size_t   used_bytes;
    size_t   used_msgs;
    element *head;
};

neu_mem_cache_t *neu_mem_cache_create(const size_t max_bytes,
                                      const size_t max_msgs)
{
    neu_mem_cache_t *cache = calloc(1, sizeof(neu_mem_cache_t));
    if (NULL == cache) {
        return NULL;
    }

    cache->max_bytes  = max_bytes;
    cache->max_msgs   = max_msgs;
    cache->used_bytes = 0;
    cache->used_msgs  = 0;
    cache->head       = NULL;
    return cache;
}

int neu_mem_cache_append(neu_mem_cache_t *cache, void *data, const size_t *size)
{
    (void) cache;
    (void) data;
    (void) size;
    return 0;
}

void *neu_mem_cache_last(neu_mem_cache_t *cache)
{
    (void) cache;
    return NULL;
}

void *neu_mem_cache_first(neu_mem_cache_t *cache)
{
    (void) cache;
    return NULL;
}

void neu_mem_cache_used(neu_mem_cache_t *cache, size_t *bytes, size_t *msgs)
{
    (void) cache;
    (void) bytes;
    (void) msgs;
}

void neu_mem_cache_destroy(neu_mem_cache_t *cache)
{
    (void) cache;
}
