/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2024 EMQ Technologies Co., Ltd All rights reserved.
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
#ifndef NEURON_UTILS_ROLLING_COUNTER_H
#define NEURON_UTILS_ROLLING_COUNTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/** Rolling counter.
 *
 * This counter is for counting values within some latest time span, like
 * network bytes sent within the last 5 seconds etc.
 */
typedef struct {
    uint64_t val;      // accumulator
    uint64_t ts;       // head time stamp in milliseconds
    uint32_t res : 21; // time resolution in milliseconds
    uint32_t hd : 5;   // head position
    uint32_t n : 6;    // number of counters
    uint32_t counts[]; // bins of counters
} neu_rolling_counter_t;

/** 创建滚动计数器。
 *
 * @param   span   时间跨度（毫秒）
 */
static inline neu_rolling_counter_t *neu_rolling_counter_new(unsigned span)
{
    unsigned n = span <= 6000 ? 4 : span <= 32000 ? 8 : span <= 64000 ? 16 : 32;
    assert(span / n < (1 << 22)); // 不应溢出

    neu_rolling_counter_t *counter = (neu_rolling_counter_t *) calloc(
        1, sizeof(*counter) + sizeof(counter->counts[0]) * n);
    if (counter) {
        counter->res = span / n;
        counter->n   = n;
    }
    return counter;
}

/** 销毁滚动计数器。
 */
static inline void neu_rolling_counter_free(neu_rolling_counter_t *counter)
{
    if (counter) {
        free(counter);
    }
}

/** 增加滚动计数器并返回值；
 *
 * @param   ts    时间戳（毫秒），应该是单调递增的
 * @param   dt    增加的增量值
 */
static inline uint64_t neu_rolling_counter_inc(neu_rolling_counter_t *counter,
                                               uint64_t ts, unsigned dt)
{
    uint64_t step = (ts - counter->ts) / counter->res;
    for (unsigned i = 0; i < step && i < counter->n; ++i) {
        counter->hd = (counter->hd + 1) & (counter->n - 1);
        counter->val -= counter->counts[counter->hd];
        counter->counts[counter->hd] = 0;
    }
    counter->val += dt;
    counter->counts[counter->hd] += dt;
    counter->ts += step * counter->res;
    return counter->val;
}

/** 重置计数器。
 */
static inline void neu_rolling_counter_reset(neu_rolling_counter_t *counter)
{
    counter->val = 0;
    counter->hd  = 0;
    memset(counter->counts, 0, counter->n * sizeof(counter->counts[0]));
}

/** 返回计数器值。
 *
 * 注意：如果计数器更新不够频繁，可能会返回过期的值。
 */
static inline uint64_t neu_rolling_counter_value(neu_rolling_counter_t *counter)
{
    return counter->val;
}

#ifdef __cplusplus
}
#endif

#endif
