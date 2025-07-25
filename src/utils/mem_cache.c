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

/**
 * 创建内存缓存
 *
 * @param max_bytes 最大字节数限制，0表示无限制
 * @param max_items 最大项目数限制，0表示无限制
 * @return 成功返回创建的内存缓存指针，失败返回NULL
 */
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

cache_item_t neu_mem_cache_earliest(neu_mem_cache_t *cache);

/**
 * 检查缓存是否已满
 *
 * @param cache 内存缓存指针
 * @param item 要添加的项目
 * @return 如果缓存已满返回true，否则返回false
 */
static bool queue_is_full(neu_mem_cache_t *cache, cache_item_t *item)
{
    if ((0 != cache->max_bytes) &&
        (item->size + cache->used_bytes) > cache->max_bytes) {
        return true;
    }

    if ((0 != cache->max_items) && (cache->used_items >= cache->max_items)) {
        return true;
    }

    return false;
}

/**
 * 减少缓存大小以便添加新项目
 *
 * @param cache 内存缓存指针
 * @param item 要添加的项目
 */
static void queue_reduce(neu_mem_cache_t *cache, cache_item_t *item)
{
    while (queue_is_full(cache, item)) {
        cache_item_t remove_item = neu_mem_cache_earliest(cache);
        if (NULL == remove_item.data) {
            break;
        }

        if (NULL != remove_item.release && NULL != remove_item.data) {
            remove_item.release(remove_item.data);
        }
    }
}

/**
 * 向内存缓存添加项目
 *
 * @param cache 内存缓存指针
 * @param item 要添加的项目
 * @return 成功返回0，失败返回-1
 */
int neu_mem_cache_add(neu_mem_cache_t *cache, cache_item_t *item)
{
    assert(NULL != cache);
    assert(NULL != item);

    queue_reduce(cache, item);
    element *el = calloc(1, sizeof(element));
    if (NULL == el) {
        return -1;
    }

    el->item = *item;
    DL_PREPEND(cache->head, el);

    cache->used_bytes += item->size;
    cache->used_items += 1;
    return 0;
}

/**
 * 获取并移除缓存中最早添加的项目
 *
 * @param cache 内存缓存指针
 * @return 返回最早添加的项目，如果缓存为空则返回空项目
 */
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

/**
 * 获取并移除缓存中最近添加的项目
 *
 * @param cache 内存缓存指针
 * @return 返回最近添加的项目，如果缓存为空则返回空项目
 */
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

/**
 * 获取缓存当前使用情况
 *
 * @param cache 内存缓存指针
 * @param bytes 输出参数，返回当前使用的字节数
 * @param msgs 输出参数，返回当前项目数
 */
void neu_mem_cache_used(neu_mem_cache_t *cache, size_t *bytes, size_t *msgs)
{
    assert(NULL != cache);

    *bytes = cache->used_bytes;
    *msgs  = cache->used_items;
}

/**
 * 调整缓存大小限制
 *
 * @param cache 内存缓存指针
 * @param new_bytes 新的字节数限制
 * @param new_items 新的项目数限制
 */
void neu_mem_cache_resize(neu_mem_cache_t *cache, size_t new_bytes,
                          size_t new_items)
{
    assert(NULL != cache);

    cache->max_bytes  = new_bytes;
    cache->max_items  = new_items;
    cache_item_t item = { 0 };
    queue_reduce(cache, &item);
}

/**
 * 导出并清空缓存中的所有项目
 *
 * @param cache 内存缓存指针
 * @param callback 处理每个项目的回调函数
 * @param ctx 传递给回调函数的上下文
 * @return 成功返回0
 */
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

/**
 * 清空缓存中的所有项目
 *
 * @param cache 内存缓存指针
 * @return 成功返回0
 */
int neu_mem_cache_clear(neu_mem_cache_t *cache)
{
    assert(NULL != cache);

    element *el  = NULL;
    element *tmp = NULL;
    DL_FOREACH_SAFE(cache->head, el, tmp)
    {
        if (NULL != el->item.release && NULL != el->item.data) {
            el->item.release(el->item.data);
        }

        LL_DELETE(cache->head, el);
        free(el);
    }

    return 0;
}

/**
 * 销毁内存缓存
 *
 * @param cache 要销毁的内存缓存指针
 */
void neu_mem_cache_destroy(neu_mem_cache_t *cache)
{
    assert(NULL != cache);

    neu_mem_cache_clear(cache);
    free(cache);
}
