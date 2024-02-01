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
    uint32_t res : 22; // time resolution in milliseconds
    uint32_t hd : 5;   // head position
    uint32_t n : 5;    // number of counters
    uint32_t counts[]; // bins of counters
} neu_rolling_counter_t;

/** Create rolling counter.
 *
 * @param   span   time span in milliseconds
 */
static inline neu_rolling_counter_t *neu_rolling_counter_new(unsigned span)
{
    unsigned n = span <= 6000 ? 4 : span <= 32000 ? 8 : span <= 64000 ? 16 : 32;
    assert(span / n < (1 << 22)); // should not overflow ti

    neu_rolling_counter_t *counter = (neu_rolling_counter_t *) calloc(
        1, sizeof(*counter) + sizeof(counter->counts[0]) * n);
    if (counter) {
        counter->res = span / n;
        counter->n   = n;
    }
    return counter;
}

/** Destructs the rolling counter.
 */
static inline void neu_rolling_counter_free(neu_rolling_counter_t *counter)
{
    if (counter) {
        free(counter);
    }
}

/** Increment the rolling counter and return the value;
 *
 * @param   ts    time stamp in milliseconds, should be monotonic
 * @param   dt    delta value to increment by
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

/** Return the counter value.
 *
 * NOTE: may return stale value if the counter is not updated frequent enough.
 */
static inline uint64_t neu_rolling_counter_value(neu_rolling_counter_t *counter)
{
    return counter->val;
}

#ifdef __cplusplus
}
#endif

#endif
