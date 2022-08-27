#include <assert.h>

#include <neuron.h>

#include "neu_mem_cache.h"

typedef struct element {
    cache_item_t    item;
    struct element *prev;
    struct element *next;
} element;

struct neu_mem_cache {
    size_t   max_bytes; // 0 no limit
    size_t   max_msgs;  // 0 no limit
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

int neu_mem_cache_add(neu_mem_cache_t *cache, cache_item_t *item)
{
    assert(NULL != cache);

    element *el = calloc(1, sizeof(element));
    if (NULL == el) {
        return -1;
    }

    el->item = *item;
    DL_PREPEND(cache->head, el);

    cache->used_bytes += item->size;
    cache->used_msgs += 1;
    return 0;
}

cache_item_t neu_mem_cache_earliest(neu_mem_cache_t *cache)
{
    assert(NULL != cache);

    cache_item_t item = { 0 };
    element *    elm  = DL_LAST(cache->head);
    if (NULL == elm) {
        return item;
    }

    item = elm->item;
    DL_DELETE(cache->head, elm);
    cache->used_bytes -= item.size;
    cache->used_msgs -= 1;
    return item;
}

cache_item_t neu_mem_cache_latest(neu_mem_cache_t *cache)
{
    assert(NULL != cache);

    cache_item_t item = { 0 };
    element *    elm  = cache->head;
    if (NULL == elm) {
        return item;
    }

    item = elm->item;
    DL_DELETE(cache->head, elm);
    cache->used_bytes -= item.size;
    cache->used_msgs -= 1;
    return item;
}

void neu_mem_cache_used(neu_mem_cache_t *cache, size_t *bytes, size_t *msgs)
{
    assert(NULL != cache);

    *bytes = cache->used_bytes;
    *msgs  = cache->used_msgs;
}

int neu_mem_cache_dump(neu_mem_cache_t *cache, void *callback, void *ctx)
{
    assert(NULL != cache);

    (void) callback;
    (void) ctx;

    return 0;
}

int neu_mem_cache_clear(neu_mem_cache_t *cache)
{
    assert(NULL != cache);

    element *el  = NULL;
    element *tmp = NULL;
    DL_FOREACH_SAFE(cache->head, el, tmp)
    {
        if (NULL != el->item.release) {
            el->item.release(&el->item);
        }

        LL_DELETE(cache->head, el);
    }

    return 0;
}

void neu_mem_cache_destroy(neu_mem_cache_t *cache)
{
    assert(NULL != cache);

    neu_mem_cache_clear(cache);
    free(cache);
}
