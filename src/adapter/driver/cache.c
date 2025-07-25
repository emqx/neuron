/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2021 EMQ Technologies Co., Ltd All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 **/

#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <string.h>

#include "utils/uthash.h"

#include "define.h"
#include "tag.h"

#include "cache.h"

typedef struct {
    char group[NEU_GROUP_NAME_LEN];
    char tag[NEU_TAG_NAME_LEN];
} tkey_t;

struct elem {
    int64_t timestamp;
    bool    changed;

    neu_dvalue_t value;

    neu_tag_meta_t metas[NEU_TAG_META_SIZE];

    tkey_t         key;
    UT_hash_handle hh;
};

struct neu_driver_cache {
    pthread_mutex_t mtx;

    struct elem *table;
};

/**
 * 将组名和标签名转换为缓存中的键
 *
 * @param group 组名
 * @param tag 标签名
 * @return 生成的键结构体
 */
inline static tkey_t to_key(const char *group, const char *tag)
{
    tkey_t key = { 0 };

    strcpy(key.group, group);
    strcpy(key.tag, tag);

    return key;
}

/**
 * 创建新的驱动缓存实例
 *
 * @return 新创建的驱动缓存实例
 */
neu_driver_cache_t *neu_driver_cache_new()
{
    neu_driver_cache_t *cache = calloc(1, sizeof(neu_driver_cache_t));

    pthread_mutex_init(&cache->mtx, NULL);

    return cache;
}

/**
 * 销毁驱动缓存实例，释放所有资源
 *
 * @param cache 驱动缓存实例
 */
void neu_driver_cache_destroy(neu_driver_cache_t *cache)
{
    struct elem *elem = NULL;
    struct elem *tmp  = NULL;

    pthread_mutex_lock(&cache->mtx);
    HASH_ITER(hh, cache->table, elem, tmp)
    {
        HASH_DEL(cache->table, elem);
        if (elem->value.type == NEU_TYPE_PTR) {
            if (elem->value.value.ptr.ptr != NULL) {
                free(elem->value.value.ptr.ptr);
                elem->value.value.ptr.ptr = NULL;
            }
        }
        free(elem);
    }
    pthread_mutex_unlock(&cache->mtx);

    pthread_mutex_destroy(&cache->mtx);

    free(cache);
}

/**
 * 添加标签到缓存
 *
 * @param cache 驱动缓存实例
 * @param group 组名
 * @param tag 标签名
 * @param value 标签初始值
 */
void neu_driver_cache_add(neu_driver_cache_t *cache, const char *group,
                          const char *tag, neu_dvalue_t value)
{
    struct elem *elem = NULL;
    tkey_t       key  = to_key(group, tag);

    pthread_mutex_lock(&cache->mtx);
    HASH_FIND(hh, cache->table, &key, sizeof(tkey_t), elem);

    if (elem == NULL) {
        elem = calloc(1, sizeof(struct elem));

        strcpy(elem->key.group, group);
        strcpy(elem->key.tag, tag);

        HASH_ADD(hh, cache->table, key, sizeof(tkey_t), elem);
    }

    elem->timestamp = 0;
    elem->changed   = false;
    elem->value     = value;

    pthread_mutex_unlock(&cache->mtx);
}

/**
 * 更新缓存中的标签值并控制变更标志
 *
 * @param cache 驱动缓存实例
 * @param group 组名
 * @param tag 标签名
 * @param timestamp 时间戳
 * @param value 新值
 * @param metas 元数据数组
 * @param n_meta 元数据数量
 * @param change 是否强制标记为已变更
 */
void neu_driver_cache_update_change(neu_driver_cache_t *cache,
                                    const char *group, const char *tag,
                                    int64_t timestamp, neu_dvalue_t value,
                                    neu_tag_meta_t *metas, int n_meta,
                                    bool change)
{
    struct elem *elem = NULL;
    tkey_t       key  = to_key(group, tag);

    pthread_mutex_lock(&cache->mtx);
    HASH_FIND(hh, cache->table, &key, sizeof(tkey_t), elem);
    if (elem != NULL) {
        elem->timestamp = timestamp;
        if (elem->value.type != value.type) {
            elem->changed = true;
        } else {
            switch (value.type) {
            case NEU_TYPE_INT8:
            case NEU_TYPE_UINT8:
            case NEU_TYPE_INT16:
            case NEU_TYPE_UINT16:
            case NEU_TYPE_INT32:
            case NEU_TYPE_UINT32:
            case NEU_TYPE_INT64:
            case NEU_TYPE_UINT64:
            case NEU_TYPE_BIT:
            case NEU_TYPE_BOOL:
            case NEU_TYPE_STRING:
            case NEU_TYPE_WORD:
            case NEU_TYPE_DWORD:
            case NEU_TYPE_LWORD:
                if (memcmp(&elem->value.value, &value.value,
                           sizeof(value.value)) != 0) {
                    elem->changed = true;
                }
                break;
            case NEU_TYPE_BYTES:
                if (elem->value.value.bytes.length !=
                    value.value.bytes.length) {
                    elem->changed = true;
                } else {
                    if (memcpy(elem->value.value.bytes.bytes,
                               value.value.bytes.bytes,
                               value.value.bytes.length) != 0) {
                        elem->changed = true;
                    }
                }
                break;
            case NEU_TYPE_PTR: {
                if (elem->value.value.ptr.length != value.value.ptr.length) {
                    elem->changed = true;
                } else {
                    if (memcmp(elem->value.value.ptr.ptr, value.value.ptr.ptr,
                               value.value.ptr.length) != 0) {
                        elem->changed = true;
                    }
                }

                break;
            }
            case NEU_TYPE_FLOAT:
                if (elem->value.precision == 0) {
                    elem->changed = elem->value.value.f32 != value.value.f32;
                } else {
                    if (fabs(elem->value.value.f32 - value.value.f32) >
                        pow(0.1, elem->value.precision)) {
                        elem->changed = true;
                    }
                }
                break;
            case NEU_TYPE_DOUBLE:
                if (elem->value.precision == 0) {
                    elem->changed = elem->value.value.d64 != value.value.d64;
                } else {
                    if (fabs(elem->value.value.d64 - value.value.d64) >
                        pow(0.1, elem->value.precision)) {
                        elem->changed = true;
                    }
                }

                break;
            case NEU_TYPE_ERROR:
                elem->changed = true;
                break;
            }
        }

        if (change) {
            elem->changed = true;
        }

        elem->value.type = value.type;
        if (elem->value.type == NEU_TYPE_PTR) {
            elem->value.value.ptr.length = value.value.ptr.length;
            elem->value.value.ptr.type   = value.value.ptr.type;
            if (elem->value.value.ptr.ptr != NULL) {
                free(elem->value.value.ptr.ptr);
            }
            elem->value.value.ptr.ptr = calloc(1, value.value.ptr.length);
            memcpy(elem->value.value.ptr.ptr, value.value.ptr.ptr,
                   value.value.ptr.length);
        } else {
            elem->value.value = value.value;
        }

        memset(elem->metas, 0, sizeof(neu_tag_meta_t) * NEU_TAG_META_SIZE);
        for (int i = 0; i < n_meta; i++) {
            memcpy(&elem->metas[i], &metas[i], sizeof(neu_tag_meta_t));
        }
    }

    pthread_mutex_unlock(&cache->mtx);
}

/**
 * 更新缓存中的标签值
 *
 * @param cache 驱动缓存实例
 * @param group 组名
 * @param tag 标签名
 * @param timestamp 时间戳
 * @param value 新值
 * @param metas 元数据数组
 * @param n_meta 元数据数量
 */
void neu_driver_cache_update(neu_driver_cache_t *cache, const char *group,
                             const char *tag, int64_t timestamp,
                             neu_dvalue_t value, neu_tag_meta_t *metas,
                             int n_meta)
{
    neu_driver_cache_update_change(cache, group, tag, timestamp, value, metas,
                                   n_meta, false);
}

/**
 * 获取缓存中的标签值和元数据
 *
 * @param cache 驱动缓存实例
 * @param group 组名
 * @param tag 标签名
 * @param value 输出参数，存放获取的值
 * @param metas 输出参数，存放获取的元数据
 * @param n_meta 元数据缓冲区大小
 * @return 0表示成功，-1表示标签不存在
 */
int neu_driver_cache_meta_get(neu_driver_cache_t *cache, const char *group,
                              const char *tag, neu_driver_cache_value_t *value,
                              neu_tag_meta_t *metas, int n_meta)
{
    struct elem *elem = NULL;
    int          ret  = -1;
    tkey_t       key  = to_key(group, tag);

    pthread_mutex_lock(&cache->mtx);
    HASH_FIND(hh, cache->table, &key, sizeof(tkey_t), elem);

    if (elem != NULL) {
        value->timestamp       = elem->timestamp;
        value->value.type      = elem->value.type;
        value->value.precision = elem->value.precision;

        assert(n_meta <= NEU_TAG_META_SIZE);
        memcpy(metas, elem->metas, sizeof(neu_tag_meta_t) * NEU_TAG_META_SIZE);

        switch (elem->value.type) {
        case NEU_TYPE_INT8:
        case NEU_TYPE_UINT8:
        case NEU_TYPE_BIT:
            value->value.value.u8 = elem->value.value.u8;
            break;
        case NEU_TYPE_INT16:
        case NEU_TYPE_UINT16:
        case NEU_TYPE_WORD:
            value->value.value.u16 = elem->value.value.u16;
            break;
        case NEU_TYPE_INT32:
        case NEU_TYPE_UINT32:
        case NEU_TYPE_DWORD:
        case NEU_TYPE_FLOAT:
        case NEU_TYPE_ERROR:
            value->value.value.u32 = elem->value.value.u32;
            break;
        case NEU_TYPE_INT64:
        case NEU_TYPE_UINT64:
        case NEU_TYPE_DOUBLE:
        case NEU_TYPE_LWORD:
            value->value.value.u64 = elem->value.value.u64;
            break;
        case NEU_TYPE_BOOL:
            value->value.value.boolean = elem->value.value.boolean;
            break;
        case NEU_TYPE_STRING:
            memcpy(value->value.value.str, elem->value.value.str,
                   sizeof(elem->value.value.str));
            break;
        case NEU_TYPE_BYTES:
            value->value.value.bytes.length = elem->value.value.bytes.length;
            memcpy(value->value.value.bytes.bytes,
                   elem->value.value.bytes.bytes,
                   elem->value.value.bytes.length);
            break;
        case NEU_TYPE_PTR:
            value->value.value.ptr.length = elem->value.value.ptr.length;
            value->value.value.ptr.type   = elem->value.value.ptr.type;
            value->value.value.ptr.ptr =
                calloc(1, elem->value.value.ptr.length);
            memcpy(value->value.value.ptr.ptr, elem->value.value.ptr.ptr,
                   elem->value.value.ptr.length);
            break;
        }

        for (int i = 0; i < NEU_TAG_META_SIZE; i++) {
            if (strlen(elem->metas[i].name) > 0) {
                memcpy(&value->metas[i], &elem->metas[i],
                       sizeof(neu_tag_meta_t));
            }
        }

        ret = 0;
    }

    pthread_mutex_unlock(&cache->mtx);

    return ret;
}

/**
 * 获取缓存中已变更的标签值和元数据
 *
 * @param cache 驱动缓存实例
 * @param group 组名
 * @param tag 标签名
 * @param value 输出参数，存放获取的值
 * @param metas 输出参数，存放获取的元数据
 * @param n_meta 元数据缓冲区大小
 * @return 0表示成功且有变更，-1表示标签不存在或无变更
 */
int neu_driver_cache_meta_get_changed(neu_driver_cache_t *cache,
                                      const char *group, const char *tag,
                                      neu_driver_cache_value_t *value,
                                      neu_tag_meta_t *metas, int n_meta)
{
    struct elem *elem = NULL;
    int          ret  = -1;
    tkey_t       key  = to_key(group, tag);

    pthread_mutex_lock(&cache->mtx);
    HASH_FIND(hh, cache->table, &key, sizeof(tkey_t), elem);

    if (elem != NULL && elem->changed) {
        value->timestamp       = elem->timestamp;
        value->value.type      = elem->value.type;
        value->value.precision = elem->value.precision;

        assert(n_meta <= NEU_TAG_META_SIZE);
        memcpy(metas, elem->metas, sizeof(neu_tag_meta_t) * NEU_TAG_META_SIZE);

        switch (elem->value.type) {
        case NEU_TYPE_INT8:
        case NEU_TYPE_UINT8:
        case NEU_TYPE_BIT:
            value->value.value.u8 = elem->value.value.u8;
            break;
        case NEU_TYPE_INT16:
        case NEU_TYPE_UINT16:
        case NEU_TYPE_WORD:
            value->value.value.u16 = elem->value.value.u16;
            break;
        case NEU_TYPE_INT32:
        case NEU_TYPE_UINT32:
        case NEU_TYPE_FLOAT:
        case NEU_TYPE_ERROR:
        case NEU_TYPE_DWORD:
            value->value.value.u32 = elem->value.value.u32;
            break;
        case NEU_TYPE_INT64:
        case NEU_TYPE_UINT64:
        case NEU_TYPE_DOUBLE:
        case NEU_TYPE_LWORD:
            value->value.value.u64 = elem->value.value.u64;
            break;
        case NEU_TYPE_BOOL:
            value->value.value.boolean = elem->value.value.boolean;
            break;
        case NEU_TYPE_STRING:
            memcpy(value->value.value.str, elem->value.value.str,
                   sizeof(elem->value.value.str));
            break;
        case NEU_TYPE_BYTES:
            value->value.value.bytes.length = elem->value.value.bytes.length;
            memcpy(value->value.value.bytes.bytes,
                   elem->value.value.bytes.bytes,
                   elem->value.value.bytes.length);
            break;
        case NEU_TYPE_PTR:
            value->value.value.ptr.length = elem->value.value.ptr.length;
            value->value.value.ptr.type   = elem->value.value.ptr.type;
            value->value.value.ptr.ptr =
                calloc(1, elem->value.value.ptr.length);
            memcpy(value->value.value.ptr.ptr, elem->value.value.ptr.ptr,
                   elem->value.value.ptr.length);
            break;
        }

        for (int i = 0; i < NEU_TAG_META_SIZE; i++) {
            if (strlen(elem->metas[i].name) > 0) {
                memcpy(&value->metas[i], &elem->metas[i],
                       sizeof(neu_tag_meta_t));
            }
        }

        if (elem->value.type != NEU_TYPE_ERROR) {
            elem->changed = false;
        }
        ret = 0;
    }

    pthread_mutex_unlock(&cache->mtx);

    return ret;
}

/**
 * 获取缓存中的标签值
 *
 * @param cache 驱动缓存实例
 * @param group 组名
 * @param tag 标签名
 * @param value 输出参数，存放获取的值
 * @return 0表示成功，-1表示标签不存在
 */
int neu_driver_cache_get(neu_driver_cache_t *cache, const char *group,
                         const char *tag, neu_driver_cache_value_t *value)
{
    struct elem *elem = NULL;
    int          ret  = -1;
    tkey_t       key  = to_key(group, tag);

    pthread_mutex_lock(&cache->mtx);
    HASH_FIND(hh, cache->table, &key, sizeof(tkey_t), elem);

    if (elem != NULL) {
        value->timestamp       = elem->timestamp;
        value->value.type      = elem->value.type;
        value->value.precision = elem->value.precision;

        switch (elem->value.type) {
        case NEU_TYPE_INT8:
        case NEU_TYPE_UINT8:
        case NEU_TYPE_BIT:
            value->value.value.u8 = elem->value.value.u8;
            break;
        case NEU_TYPE_INT16:
        case NEU_TYPE_UINT16:
        case NEU_TYPE_WORD:
            value->value.value.u16 = elem->value.value.u16;
            break;
        case NEU_TYPE_INT32:
        case NEU_TYPE_UINT32:
        case NEU_TYPE_DWORD:
        case NEU_TYPE_FLOAT:
        case NEU_TYPE_ERROR:
            value->value.value.u32 = elem->value.value.u32;
            break;
        case NEU_TYPE_INT64:
        case NEU_TYPE_UINT64:
        case NEU_TYPE_DOUBLE:
        case NEU_TYPE_LWORD:
            value->value.value.u64 = elem->value.value.u64;
            break;
        case NEU_TYPE_BOOL:
            value->value.value.boolean = elem->value.value.boolean;
            break;
        case NEU_TYPE_STRING:
            memcpy(value->value.value.str, elem->value.value.str,
                   sizeof(elem->value.value.str));
            break;
        case NEU_TYPE_BYTES:
            value->value.value.bytes.length = elem->value.value.bytes.length;
            memcpy(value->value.value.bytes.bytes,
                   elem->value.value.bytes.bytes,
                   elem->value.value.bytes.length);
            break;
        case NEU_TYPE_PTR:
            value->value.value.ptr.length = elem->value.value.ptr.length;
            value->value.value.ptr.type   = elem->value.value.ptr.type;
            value->value.value.ptr.ptr =
                calloc(1, elem->value.value.ptr.length);
            memcpy(value->value.value.ptr.ptr, elem->value.value.ptr.ptr,
                   elem->value.value.ptr.length);
            break;
        }

        for (int i = 0; i < NEU_TAG_META_SIZE; i++) {
            if (strlen(elem->metas[i].name) > 0) {
                memcpy(&value->metas[i], &elem->metas[i],
                       sizeof(neu_tag_meta_t));
            }
        }

        ret = 0;
    }

    pthread_mutex_unlock(&cache->mtx);

    return ret;
}

/**
 * 获取缓存中已变更的标签值
 *
 * @param cache 驱动缓存实例
 * @param group 组名
 * @param tag 标签名
 * @param value 输出参数，存放获取的值
 * @return 0表示成功且有变更，-1表示标签不存在或无变更
 */
int neu_driver_cache_get_changed(neu_driver_cache_t *cache, const char *group,
                                 const char *              tag,
                                 neu_driver_cache_value_t *value)
{
    struct elem *elem = NULL;
    int          ret  = -1;
    tkey_t       key  = to_key(group, tag);

    pthread_mutex_lock(&cache->mtx);
    HASH_FIND(hh, cache->table, &key, sizeof(tkey_t), elem);

    if (elem != NULL && elem->changed) {
        value->timestamp       = elem->timestamp;
        value->value.type      = elem->value.type;
        value->value.precision = elem->value.precision;

        switch (elem->value.type) {
        case NEU_TYPE_INT8:
        case NEU_TYPE_UINT8:
        case NEU_TYPE_BIT:
            value->value.value.u8 = elem->value.value.u8;
            break;
        case NEU_TYPE_INT16:
        case NEU_TYPE_UINT16:
        case NEU_TYPE_WORD:
            value->value.value.u16 = elem->value.value.u16;
            break;
        case NEU_TYPE_INT32:
        case NEU_TYPE_UINT32:
        case NEU_TYPE_FLOAT:
        case NEU_TYPE_ERROR:
        case NEU_TYPE_DWORD:
            value->value.value.u32 = elem->value.value.u32;
            break;
        case NEU_TYPE_INT64:
        case NEU_TYPE_UINT64:
        case NEU_TYPE_DOUBLE:
        case NEU_TYPE_LWORD:
            value->value.value.u64 = elem->value.value.u64;
            break;
        case NEU_TYPE_BOOL:
            value->value.value.boolean = elem->value.value.boolean;
            break;
        case NEU_TYPE_STRING:
            memcpy(value->value.value.str, elem->value.value.str,
                   sizeof(elem->value.value.str));
            break;
        case NEU_TYPE_BYTES:
            value->value.value.bytes.length = elem->value.value.bytes.length;
            memcpy(value->value.value.bytes.bytes,
                   elem->value.value.bytes.bytes,
                   elem->value.value.bytes.length);
            break;
        case NEU_TYPE_PTR:
            value->value.value.ptr.length = elem->value.value.ptr.length;
            value->value.value.ptr.type   = elem->value.value.ptr.type;
            value->value.value.ptr.ptr =
                calloc(1, elem->value.value.ptr.length);
            memcpy(value->value.value.ptr.ptr, elem->value.value.ptr.ptr,
                   elem->value.value.ptr.length);
        }

        for (int i = 0; i < NEU_TAG_META_SIZE; i++) {
            if (strlen(elem->metas[i].name) > 0) {
                memcpy(&value->metas[i], &elem->metas[i],
                       sizeof(neu_tag_meta_t));
            }
        }

        if (elem->value.type != NEU_TYPE_ERROR) {
            elem->changed = false;
        }
        ret = 0;
    }

    pthread_mutex_unlock(&cache->mtx);

    return ret;
}

/**
 * 从缓存中删除标签
 *
 * @param cache 驱动缓存实例
 * @param group 组名
 * @param tag 标签名
 */
void neu_driver_cache_del(neu_driver_cache_t *cache, const char *group,
                          const char *tag)
{
    struct elem *elem = NULL;
    tkey_t       key  = to_key(group, tag);

    pthread_mutex_lock(&cache->mtx);
    HASH_FIND(hh, cache->table, &key, sizeof(tkey_t), elem);

    if (elem != NULL) {
        HASH_DEL(cache->table, elem);
        if (elem->value.type == NEU_TYPE_PTR) {
            if (elem->value.value.ptr.ptr != NULL) {
                free(elem->value.value.ptr.ptr);
                elem->value.value.ptr.ptr = NULL;
            }
        }
        free(elem);
    }

    pthread_mutex_unlock(&cache->mtx);
}
