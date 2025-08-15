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
#include <stdlib.h>

#include "tag_sort.h"

/**
 * @brief 将UT_array转换为双向链表
 *
 * @param tags 标签数组
 * @return 转换后的链表头指针
 */
static neu_tag_sort_elem_t *array_to_list(UT_array *tags);

/**
 * @brief 将标签添加到排序结果中
 *
 * @param result 排序结果结构体
 * @param tag 要添加的标签
 * @param fn 排序函数
 * @param icd UT数组的元素描述符
 */
static void tag_sort(neu_tag_sort_result_t *result, void *tag,
                     neu_tag_sort_fn fn, UT_icd icd);

/**
 * @brief 对标签进行分组排序
 *
 * 该函数将输入的标签数组按照提供的排序函数和比较函数进行分组和排序
 *
 * @param tags 要排序的标签数组
 * @param sort 排序函数，用于判断标签是否属于同一组
 * @param cmp 比较函数，用于确定标签的顺序
 * @return 排序结果结构体指针
 */
neu_tag_sort_result_t *neu_tag_sort(UT_array *tags, neu_tag_sort_fn sort,
                                    neu_tag_sort_cmp cmp)
{
    /* 分配排序结果结构体 */
    neu_tag_sort_result_t *result = calloc(1, sizeof(neu_tag_sort_result_t));
    neu_tag_sort_elem_t   *elt = NULL, *tmp = NULL;
    /* 将数组转换为链表以便排序 */
    neu_tag_sort_elem_t *head = array_to_list(tags);

    /* 使用提供的比较函数对链表进行排序 */
    DL_SORT(head, cmp);
    /* 遍历排序后的链表，将每个标签分组添加到结果中 */
    DL_FOREACH_SAFE(head, elt, tmp)
    {
        tag_sort(result, elt->tag, sort, tags->icd);
        DL_DELETE(head, elt);
        free(elt);
    }

    return result;
}

/**
 * @brief 释放排序结果结构体资源
 *
 * @param result 要释放的排序结果结构体指针
 */
void neu_tag_sort_free(neu_tag_sort_result_t *result)
{
    /* 释放每个分组中的标签数组 */
    for (uint16_t i = 0; i < result->n_sort; i++) {
        utarray_free(result->sorts[i].tags);
    }

    /* 释放分组数组和结果结构体 */
    free(result->sorts);
    free(result);
}

/**
 * @brief 将UT_array转换为双向链表
 *
 * 此函数将UT_array中的标签转换为双向链表，以便后续排序
 *
 * @param tags 标签数组
 * @return 转换后的链表头指针
 */
static neu_tag_sort_elem_t *array_to_list(UT_array *tags)
{
    neu_tag_sort_elem_t *head = NULL, *tmp = NULL;

    /* 遍历标签数组，为每个标签创建链表节点 */
    for (void **tag = utarray_front(tags); tag != NULL;
         tag        = utarray_next(tags, tag)) {
        /* 为节点分配内存 */
        tmp      = calloc(1, sizeof(neu_tag_sort_elem_t));
        tmp->tag = *tag;
        /* 将节点添加到链表末尾 */
        DL_APPEND(head, tmp);
    }

    return head;
}

/**
 * @brief 将标签添加到排序结果中
 *
 * 该函数尝试将标签添加到已有的分组中，如果没有合适的分组则创建新分组
 *
 * @param result 排序结果结构体
 * @param tag 要添加的标签
 * @param fn 排序函数，用于判断标签是否属于同一组
 * @param icd UT数组的元素描述符
 */
static void tag_sort(neu_tag_sort_result_t *result, void *tag,
                     neu_tag_sort_fn fn, UT_icd icd)
{
    bool sorted = false;

    /* 尝试将标签添加到已有的分组中 */
    for (uint16_t i = 0; i < result->n_sort; i++) {
        /* 使用排序函数判断标签是否属于当前分组 */
        if (fn(&result->sorts[i],
               *(void **) utarray_back(result->sorts[i].tags), tag)) {
            /* 将标签添加到当前分组 */
            utarray_push_back(result->sorts[i].tags, &tag);
            result->sorts[i].info.size += 1;
            sorted = true;
            break;
        }
    }

    /* 如果标签不属于任何已有分组，则创建新分组 */
    if (!sorted) {
        /* 增加分组计数 */
        result->n_sort += 1;
        /* 重新分配分组数组 */
        result->sorts =
            realloc(result->sorts, sizeof(neu_tag_sort_t) * result->n_sort);

        /* 初始化新分组 */
        memset(&result->sorts[result->n_sort - 1], 0, sizeof(neu_tag_sort_t));
        utarray_new(result->sorts[result->n_sort - 1].tags, &icd);
        utarray_push_back(result->sorts[result->n_sort - 1].tags, &tag);
        result->sorts[result->n_sort - 1].info.size = 1;
        /* 调用排序函数初始化分组信息 */
        fn(&result->sorts[result->n_sort - 1],
           *(void **) utarray_back(result->sorts[result->n_sort - 1].tags),
           tag);
    }
}