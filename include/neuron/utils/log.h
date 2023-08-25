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
config_ **/

#ifndef _NEU_LOG_H_
#define _NEU_LOG_H_

#include <inttypes.h>

#include "utils/zlog.h"

extern zlog_category_t *neuron;

#define nlog_fatal(...)                                    \
    zlog(neuron, __FILE__, sizeof(__FILE__) - 1, __func__, \
         sizeof(__func__) - 1, __LINE__, ZLOG_LEVEL_FATAL, __VA_ARGS__)
#define nlog_error(...)                                    \
    zlog(neuron, __FILE__, sizeof(__FILE__) - 1, __func__, \
         sizeof(__func__) - 1, __LINE__, ZLOG_LEVEL_ERROR, __VA_ARGS__)
#define nlog_warn(...)                                     \
    zlog(neuron, __FILE__, sizeof(__FILE__) - 1, __func__, \
         sizeof(__func__) - 1, __LINE__, ZLOG_LEVEL_WARN, __VA_ARGS__)
#define nlog_notice(...)                                   \
    zlog(neuron, __FILE__, sizeof(__FILE__) - 1, __func__, \
         sizeof(__func__) - 1, __LINE__, ZLOG_LEVEL_NOTICE, __VA_ARGS__)
#define nlog_info(...)                                     \
    zlog(neuron, __FILE__, sizeof(__FILE__) - 1, __func__, \
         sizeof(__func__) - 1, __LINE__, ZLOG_LEVEL_INFO, __VA_ARGS__)
#define nlog_debug(...)                                    \
    zlog(neuron, __FILE__, sizeof(__FILE__) - 1, __func__, \
         sizeof(__func__) - 1, __LINE__, ZLOG_LEVEL_DEBUG, __VA_ARGS__)

#define plog_fatal(plugin, ...)                                          \
    zlog((plugin)->common.log, __FILE__, sizeof(__FILE__) - 1, __func__, \
         sizeof(__func__) - 1, __LINE__, ZLOG_LEVEL_FATAL, __VA_ARGS__)
#define plog_error(plugin, ...)                                          \
    zlog((plugin)->common.log, __FILE__, sizeof(__FILE__) - 1, __func__, \
         sizeof(__func__) - 1, __LINE__, ZLOG_LEVEL_ERROR, __VA_ARGS__)
#define plog_warn(plugin, ...)                                           \
    zlog((plugin)->common.log, __FILE__, sizeof(__FILE__) - 1, __func__, \
         sizeof(__func__) - 1, __LINE__, ZLOG_LEVEL_WARN, __VA_ARGS__)
#define plog_notice(plugin, ...)                                         \
    zlog((plugin)->common.log, __FILE__, sizeof(__FILE__) - 1, __func__, \
         sizeof(__func__) - 1, __LINE__, ZLOG_LEVEL_NOTICE, __VA_ARGS__)
#define plog_info(plugin, ...)                                           \
    zlog((plugin)->common.log, __FILE__, sizeof(__FILE__) - 1, __func__, \
         sizeof(__func__) - 1, __LINE__, ZLOG_LEVEL_INFO, __VA_ARGS__)
#define plog_debug(plugin, ...)                                          \
    zlog((plugin)->common.log, __FILE__, sizeof(__FILE__) - 1, __func__, \
         sizeof(__func__) - 1, __LINE__, ZLOG_LEVEL_DEBUG, __VA_ARGS__)

#define plog_send_protocol(plugin, bytes, n_byte)                            \
    uint16_t log_protocol_buf_size = n_byte * 5 + 20;                        \
    char *   log_protocol_buf      = calloc(log_protocol_buf_size, 1);       \
    int      log_offset            = 0;                                      \
    uint16_t tmp_n_byte            = n_byte;                                 \
    log_offset = snprintf(log_protocol_buf, log_protocol_buf_size, ">>(%d)", \
                          tmp_n_byte);                                       \
    for (int i = 0; i < n_byte; i++) {                                       \
        log_offset += snprintf(log_protocol_buf + log_offset,                \
                               log_protocol_buf_size - log_offset,           \
                               " 0x%02hhX", (bytes)[i]);                     \
    }                                                                        \
    plog_debug(plugin, "%s", log_protocol_buf);                              \
    free(log_protocol_buf);

#define plog_recv_protocol(plugin, bytes, n_byte)                            \
    uint16_t log_protocol_buf_size = n_byte * 5 + 20;                        \
    char *   log_protocol_buf      = calloc(log_protocol_buf_size, 1);       \
    int      log_offset            = 0;                                      \
    uint16_t tmp_n_byte            = n_byte;                                 \
    log_offset = snprintf(log_protocol_buf, log_protocol_buf_size, "<<(%d)", \
                          tmp_n_byte);                                       \
    for (int i = 0; i < n_byte; i++) {                                       \
        log_offset += snprintf(log_protocol_buf + log_offset,                \
                               log_protocol_buf_size - log_offset,           \
                               " 0x%02hhX", (bytes)[i]);                     \
    }                                                                        \
    plog_debug(plugin, "%s", log_protocol_buf);                              \
    free(log_protocol_buf);

#define zlog_recv_protocol(log, bytes, n_byte)                               \
    uint16_t log_protocol_buf_size = n_byte * 5 + 20;                        \
    char *   log_protocol_buf      = calloc(log_protocol_buf_size, 1);       \
    int      log_offset            = 0;                                      \
    uint16_t tmp_n_byte            = n_byte;                                 \
    log_offset = snprintf(log_protocol_buf, log_protocol_buf_size, "<<(%d)", \
                          tmp_n_byte);                                       \
    for (int i = 0; i < n_byte; i++) {                                       \
        log_offset += snprintf(log_protocol_buf + log_offset,                \
                               log_protocol_buf_size - log_offset,           \
                               " 0x%02hhX", (bytes)[i]);                     \
    }                                                                        \
    zlog_debug(log, "%s", log_protocol_buf);                                 \
    free(log_protocol_buf);

void remove_logs(const char *node);

#endif