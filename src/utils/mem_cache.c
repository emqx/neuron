#include <assert.h>

#include <neuron.h>

#include "utils/mem_cache.h"

typedef struct element {
    cache_item_t    item;
    struct element *prev;
    struct element *next;
} element;

struct neu_mem_cache {
    size_t   max_bytes; // 0 no limit
    size_t   max_items; // 0 no limit
    size_t   used_bytes;
    size_t   used_items;
    element *head;
};

neu_mem_cache_t *neu_mem_cache_create(const size_t max_bytes,
                                      const size_t max_items)
{
    neu_mem_cache_t *cache = calloc(1, sizeof(neu_mem_cache_t));
    if (NULL == cache) {
        return NULL;
    }

    cache->max_bytes  = max_bytes;
    cache->max_items  = max_items;
    cache->used_bytes = 0;
    cache->used_items = 0;
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

    if ((0 != cache->used_bytes) &&
        (item->size + cache->used_bytes) > cache->max_bytes) {
        free(el);
        return -1;
    }

    if ((0 != cache->used_items) && (cache->used_items >= cache->max_bytes)) {
        free(el);
        return -1;
    }

    el->item = *item;
    DL_PREPEND(cache->head, el);

    cache->used_bytes += item->size;
    cache->used_items += 1;
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
    free(elm);

    cache->used_bytes -= item.size;
    cache->used_items -= 1;
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
    free(elm);

    cache->used_bytes -= item.size;
    cache->used_items -= 1;
    return item;
}

void neu_mem_cache_used(neu_mem_cache_t *cache, size_t *bytes, size_t *msgs)
{
    assert(NULL != cache);

    *bytes = cache->used_bytes;
    *msgs  = cache->used_items;
}

int neu_mem_cache_dump(neu_mem_cache_t *cache, cache_dump callback, void *ctx)
{
    assert(NULL != cache);

    if (NULL != callback) {
        element *el  = NULL;
        element *tmp = NULL;
        DL_FOREACH_SAFE(cache->head, el, tmp)
        {
            callback(&el->item, ctx);

            if (NULL != el->item.release) {
                el->item.release(&el->item);
            }

            LL_DELETE(cache->head, el);
        }
    }

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
        free(el);
    }

    return 0;
}

void neu_mem_cache_destroy(neu_mem_cache_t *cache)
{
    assert(NULL != cache);

    neu_mem_cache_clear(cache);
    free(cache);
}
