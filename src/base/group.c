/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2022 EMQ Technologies Co., Ltd All rights reserved.
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
#include <pthread.h>
#include <string.h>
#include <sys/time.h>

#include "define.h"
#include "errcodes.h"

#include "group.h"

/**
 * @brief 标签元素结构，用于在哈希表中存储标签信息
 */
typedef struct tag_elem {
    char          *name; /**< 标签名称，作为哈希表的键 */
    neu_datatag_t *tag;  /**< 标签详细信息 */
    UT_hash_handle hh;   /**< uthash哈希句柄 */
} tag_elem_t;

/**
 * @brief 组结构定义，用于管理一组相关的数据标签
 */
struct neu_group {
    char           *name;      /**< 组名称 */
    tag_elem_t     *tags;      /**< 包含的标签哈希表 */
    uint32_t        interval;  /**< 组轮询间隔(毫秒) */
    int64_t         timestamp; /**< 组最后修改的时间戳 */
    pthread_mutex_t mtx;       /**< 互斥锁，保护组内数据的并发访问 */
};

/* 函数声明 */
static UT_array *to_array(tag_elem_t *tags);      /**< 将标签哈希表转换为数组 */
static UT_array *to_read_array(tag_elem_t *tags); /**< 提取可读标签到数组 */
static void
            split_static_array(tag_elem_t *tags, UT_array **static_tags,
                               UT_array **other_tags); /**< 分离静态标签和其他标签 */
static void update_timestamp(neu_group_t *group);      /**< 更新组的时间戳 */

/**
 * @brief 创建新的组实例
 *
 * @param name 组名称
 * @param interval 组轮询间隔(毫秒)
 * @return 成功返回组实例指针，失败返回NULL
 */
neu_group_t *neu_group_new(const char *name, uint32_t interval)
{
    neu_group_t *group = calloc(1, sizeof(neu_group_t));

    group->name     = strdup(name);        /* 复制组名称 */
    group->interval = interval;            /* 设置轮询间隔 */
    pthread_mutex_init(&group->mtx, NULL); /* 初始化互斥锁 */

    return group;
}

/**
 * @brief 销毁组实例并释放所有资源
 *
 * @param group 要销毁的组实例
 */
void neu_group_destroy(neu_group_t *group)
{
    tag_elem_t *el = NULL, *tmp = NULL;

    pthread_mutex_lock(&group->mtx);
    /* 遍历并释放所有标签元素 */
    HASH_ITER(hh, group->tags, el, tmp)
    {
        HASH_DEL(group->tags, el); /* 从哈希表中移除 */
        free(el->name);            /* 释放标签名称 */
        neu_tag_free(el->tag);     /* 释放标签资源 */
        free(el);                  /* 释放标签元素结构 */
    }
    pthread_mutex_unlock(&group->mtx);

    pthread_mutex_destroy(&group->mtx); /* 销毁互斥锁 */
    free(group->name);                  /* 释放组名称 */
    free(group);                        /* 释放组结构 */
}

/**
 * @brief 获取组的名称
 *
 * @param group 组实例
 * @return 组名称
 */
const char *neu_group_get_name(const neu_group_t *group)
{
    return group->name;
}

/**
 * @brief 设置组的名称
 *
 * @param group 组实例
 * @param name 新的组名称
 * @return 成功返回0，失败返回错误码
 */
int neu_group_set_name(neu_group_t *group, const char *name)
{
    char *new_name = NULL;
    if (NULL == name || NULL == (new_name = strdup(name))) {
        return NEU_ERR_EINTERNAL; /* 内存分配失败 */
    }

    free(group->name);      /* 释放旧名称 */
    group->name = new_name; /* 设置新名称 */
    return 0;
}

/**
 * @brief 获取组的轮询间隔
 *
 * @param group 组实例
 * @return 轮询间隔(毫秒)
 */
uint32_t neu_group_get_interval(const neu_group_t *group)
{
    uint32_t interval = 0;

    interval = group->interval;

    return interval;
}

/**
 * @brief 设置组的轮询间隔
 *
 * @param group 组实例
 * @param interval 新的轮询间隔(毫秒)
 */
void neu_group_set_interval(neu_group_t *group, uint32_t interval)
{
    group->interval = interval;
}

/**
 * @brief 更新组的轮询间隔，并在发生变化时更新时间戳
 *
 * @param group 组实例
 * @param interval 新的轮询间隔(毫秒)
 * @return 成功返回0
 */
int neu_group_update(neu_group_t *group, uint32_t interval)
{
    if (group->interval != interval) { /* 间隔发生变化 */
        group->interval = interval;    /* 更新间隔 */
        update_timestamp(group);       /* 更新时间戳 */
    }

    return 0;
}

/**
 * @brief 向组中添加标签
 *
 * @param group 组实例
 * @param tag 要添加的标签
 * @return 成功返回0，失败返回错误码
 */
int neu_group_add_tag(neu_group_t *group, const neu_datatag_t *tag)
{
    tag_elem_t *el = NULL;

    pthread_mutex_lock(&group->mtx);
    /* 检查是否已存在同名标签 */
    HASH_FIND_STR(group->tags, tag->name, el);
    if (el != NULL) {
        pthread_mutex_unlock(&group->mtx);
        return NEU_ERR_TAG_NAME_CONFLICT; /* 标签名称冲突 */
    }

    /* 创建新的标签元素 */
    el       = calloc(1, sizeof(tag_elem_t));
    el->name = strdup(tag->name);
    el->tag  = neu_tag_dup(tag); /* 复制标签内容 */

    /* 添加到哈希表 */
    HASH_ADD_STR(group->tags, name, el);
    update_timestamp(group); /* 更新组时间戳 */
    pthread_mutex_unlock(&group->mtx);

    return 0;
}

/**
 * @brief 更新组中的标签
 *
 * @param group 组实例
 * @param tag 包含更新信息的标签
 * @return 成功返回NEU_ERR_SUCCESS，未找到标签返回NEU_ERR_TAG_NOT_EXIST
 */
int neu_group_update_tag(neu_group_t *group, const neu_datatag_t *tag)
{
    tag_elem_t *el  = NULL;
    int         ret = NEU_ERR_TAG_NOT_EXIST;

    pthread_mutex_lock(&group->mtx);
    /* 查找要更新的标签 */
    HASH_FIND_STR(group->tags, tag->name, el);
    if (el != NULL) {
        neu_tag_copy(el->tag, tag); /* 复制新的标签内容 */

        update_timestamp(group); /* 更新组时间戳 */
        ret = NEU_ERR_SUCCESS;
    }
    pthread_mutex_unlock(&group->mtx);

    return ret;
}

/**
 * @brief 从组中删除标签
 *
 * @param group 组实例
 * @param tag_name 要删除的标签名称
 * @return 成功返回NEU_ERR_SUCCESS，未找到标签返回NEU_ERR_TAG_NOT_EXIST
 */
int neu_group_del_tag(neu_group_t *group, const char *tag_name)
{
    tag_elem_t *el  = NULL;
    int         ret = NEU_ERR_TAG_NOT_EXIST;

    pthread_mutex_lock(&group->mtx);
    /* 查找要删除的标签 */
    HASH_FIND_STR(group->tags, tag_name, el);
    if (el != NULL) {
        /* 从哈希表中删除 */
        HASH_DEL(group->tags, el);
        /* 释放资源 */
        free(el->name);
        neu_tag_free(el->tag);
        free(el);

        update_timestamp(group); /* 更新组时间戳 */
        ret = NEU_ERR_SUCCESS;
    }
    pthread_mutex_unlock(&group->mtx);

    return ret;
}

/**
 * @brief 获取组中的所有标签
 *
 * @param group 组实例
 * @return 包含所有标签的数组
 */
UT_array *neu_group_get_tag(neu_group_t *group)
{
    UT_array *array = NULL;

    pthread_mutex_lock(&group->mtx);
    array = to_array(group->tags); /* 将哈希表转换为数组 */
    pthread_mutex_unlock(&group->mtx);

    return array;
}

/**
 * @brief 查询组中包含指定名称的标签
 *
 * 根据给定的名称字符串查询标签，匹配所有包含该字符串的标签名
 *
 * @param group 组实例
 * @param name 要查询的名称字符串
 * @return 匹配的标签数组
 */
UT_array *neu_group_query_tag(neu_group_t *group, const char *name)
{
    tag_elem_t *el = NULL, *tmp = NULL;
    UT_array   *array = NULL;

    pthread_mutex_lock(&group->mtx);
    /* 创建新的标签数组 */
    utarray_new(array, neu_tag_get_icd());
    /* 遍历所有标签 */
    HASH_ITER(hh, group->tags, el, tmp)
    {
        /* 检查标签名是否包含查询字符串 */
        if (strstr(el->tag->name, name) != NULL) {
            utarray_push_back(array, el->tag); /* 添加匹配的标签 */
        }
    }
    pthread_mutex_unlock(&group->mtx);

    return array;
}

/**
 * @brief 获取组中所有可读标签
 *
 * 获取具有读取、订阅或静态属性的标签
 *
 * @param group 组实例
 * @return 可读标签数组
 */
UT_array *neu_group_get_read_tag(neu_group_t *group)
{
    UT_array *array = NULL;

    pthread_mutex_lock(&group->mtx);
    array = to_read_array(group->tags); /* 获取可读标签 */
    pthread_mutex_unlock(&group->mtx);

    return array;
}

/**
 * @brief 获取组中标签的数量
 *
 * @param group 组实例
 * @return 标签数量
 */
uint16_t neu_group_tag_size(const neu_group_t *group)
{
    uint16_t size = 0;

    /* 获取哈希表中的元素数量 */
    size = HASH_COUNT(group->tags);

    return size;
}

/**
 * @brief 在组中查找特定标签
 *
 * 根据标签名精确查找标签，并返回其副本
 *
 * @param group 组实例
 * @param tag 要查找的标签名
 * @return 找到则返回标签副本，否则返回NULL
 */
neu_datatag_t *neu_group_find_tag(neu_group_t *group, const char *tag)
{
    tag_elem_t    *find   = NULL;
    neu_datatag_t *result = NULL;

    pthread_mutex_lock(&group->mtx);
    /* 根据名称精确查找标签 */
    HASH_FIND_STR(group->tags, tag, find);
    if (find != NULL) {
        result = neu_tag_dup(find->tag); /* 创建标签副本 */
    }
    pthread_mutex_unlock(&group->mtx);

    return result;
}

/**
 * @brief 将组中的标签分为静态标签和其他标签
 *
 * @param group 组实例
 * @param static_tags 用于返回静态标签数组
 * @param other_tags 用于返回其他标签数组
 */
void neu_group_split_static_tags(neu_group_t *group, UT_array **static_tags,
                                 UT_array **other_tags)
{
    /* 调用内部函数分离静态标签和其他标签 */
    return split_static_array(group->tags, static_tags, other_tags);
}

/**
 * @brief 检测组是否发生变化并执行回调
 *
 * 比较组的时间戳与给定时间戳，如有变化则执行回调函数
 *
 * @param group 组实例
 * @param timestamp 比较的时间戳
 * @param arg 传递给回调函数的参数
 * @param fn 回调函数
 */
void neu_group_change_test(neu_group_t *group, int64_t timestamp, void *arg,
                           neu_group_change_fn fn)
{
    /* 检查时间戳是否变化 */
    if (group->timestamp != timestamp) {
        UT_array *static_tags = NULL, *other_tags = NULL;
        /* 分离静态标签和其他标签 */
        split_static_array(group->tags, &static_tags, &other_tags);
        /* 执行回调函数 */
        fn(arg, group->timestamp, static_tags, other_tags, group->interval);
    }
}

/**
 * @brief 检查组是否已变更
 *
 * @param group 组实例
 * @param timestamp 比较的时间戳
 * @return 如果组时间戳与给定时间戳不同，返回true
 */
bool neu_group_is_change(neu_group_t *group, int64_t timestamp)
{
    bool change = false;

    change = group->timestamp != timestamp;

    return change;
}

/**
 * @brief 更新组的时间戳为当前时间
 *
 * @param group 组实例
 */
static void update_timestamp(neu_group_t *group)
{
    struct timeval tv = { 0 };

    /* 获取当前时间 */
    gettimeofday(&tv, NULL);

    /* 计算微秒级时间戳 */
    group->timestamp = (int64_t) tv.tv_sec * 1000 * 1000 + (int64_t) tv.tv_usec;
}

/**
 * @brief 将标签哈希表转换为数组
 *
 * @param tags 标签哈希表
 * @return 包含所有标签的数组
 */
static UT_array *to_array(tag_elem_t *tags)
{
    tag_elem_t *el = NULL, *tmp = NULL;
    UT_array   *array = NULL;

    /* 创建新数组 */
    utarray_new(array, neu_tag_get_icd());
    /* 遍历哈希表并添加到数组 */
    HASH_ITER(hh, tags, el, tmp)
    {
        utarray_push_back(array, el->tag);
    }

    return array;
}

/**
 * @brief 提取可读标签到数组
 *
 * 获取具有读取、订阅或静态属性的标签
 *
 * @param tags 标签哈希表
 * @return 可读标签数组
 */
static UT_array *to_read_array(tag_elem_t *tags)
{
    tag_elem_t *el = NULL, *tmp = NULL;
    UT_array   *array = NULL;

    /* 创建新数组 */
    utarray_new(array, neu_tag_get_icd());
    /* 遍历哈希表 */
    HASH_ITER(hh, tags, el, tmp)
    {
        /* 检查标签是否具有可读属性 */
        if (neu_tag_attribute_test(el->tag, NEU_ATTRIBUTE_READ) ||
            neu_tag_attribute_test(el->tag, NEU_ATTRIBUTE_SUBSCRIBE) ||
            neu_tag_attribute_test(el->tag, NEU_ATTRIBUTE_STATIC)) {
            utarray_push_back(array, el->tag); /* 添加到数组 */
        }
    }

    return array;
}

/**
 * @brief 将标签分为静态标签和其他标签
 *
 * 静态标签具有NEU_ATTRIBUTE_STATIC属性
 * 其他标签包括具有NEU_ATTRIBUTE_SUBSCRIBE或NEU_ATTRIBUTE_READ属性的标签
 *
 * @param tags 标签哈希表
 * @param static_tags 用于返回静态标签数组
 * @param other_tags 用于返回其他标签数组
 */
static void split_static_array(tag_elem_t *tags, UT_array **static_tags,
                               UT_array **other_tags)
{
    tag_elem_t *el = NULL, *tmp = NULL;

    /* 创建两个新数组 */
    utarray_new(*static_tags, neu_tag_get_icd());
    utarray_new(*other_tags, neu_tag_get_icd());

    /* 遍历标签哈希表 */
    HASH_ITER(hh, tags, el, tmp)
    {
        /* 根据标签属性分类 */
        if (neu_tag_attribute_test(el->tag, NEU_ATTRIBUTE_STATIC)) {
            /* 静态标签 */
            utarray_push_back(*static_tags, el->tag);
        } else if (neu_tag_attribute_test(el->tag, NEU_ATTRIBUTE_SUBSCRIBE) ||
                   neu_tag_attribute_test(el->tag, NEU_ATTRIBUTE_READ)) {
            /* 可订阅或可读标签 */
            utarray_push_back(*other_tags, el->tag);
        }
    }
}
