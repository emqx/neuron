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
#ifndef __NEU_TIME_H__
#define __NEU_TIME_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

/**
 * @brief 获取当前系统时间的毫秒时间戳
 *
 * @return int64_t 返回自Unix纪元（1970-01-01 00:00:00 UTC）以来的毫秒数
 */
static inline int64_t neu_time_ms()
{
    struct timeval tv = { 0 };
    gettimeofday(&tv, NULL);
    return (int64_t) tv.tv_sec * 1000 + (int64_t) tv.tv_usec / 1000;
}

/**
 * @brief 使当前线程休眠指定的毫秒数
 *
 * @param msec 休眠时间，单位为毫秒
 */
static inline void neu_msleep(unsigned msec)
{
    struct timespec tv = {
        .tv_sec  = msec / 1000,
        .tv_nsec = (msec % 1000) * 1000000,
    };
    nanosleep(&tv, NULL);
}

#ifdef __cplusplus
}
#endif

#endif
