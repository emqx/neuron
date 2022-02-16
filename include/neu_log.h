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

#ifndef NEURON_LOG_H
#define NEURON_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#define LOG_VERSION "0.1.0"

enum {
    NEU_LOG_TRACE,
    NEU_LOG_DEBUG,
    NEU_LOG_INFO,
    NEU_LOG_WARN,
    NEU_LOG_ERROR,
    NEU_LOG_FATAL
};

extern const char *neu_log_level_string(int level);
extern void        neu_log_set_level(int level);
extern void        neu_log_set_quiet(bool enable);

extern void neu_log_priv_impl_(int level, const char *file, int line,
                               const char *func, const char *label,
                               const char *fmt, ...);

/*
#define fatal(msg, rv)                             \
    {                                              \
        log_fatal("%s:%s", msg, strerror(rv)); \
        exit(1);                                   \
    }
*/

#ifndef NEURON_LOG_LABEL
#define NEURON_LOG_LABEL "neuron"
#endif

#define log_trace(...)                                                  \
    neu_log_priv_impl_(NEU_LOG_TRACE, __FILE__, __LINE__, __FUNCTION__, \
                       NEURON_LOG_LABEL, __VA_ARGS__)
#define log_debug(...)                                                  \
    neu_log_priv_impl_(NEU_LOG_DEBUG, __FILE__, __LINE__, __FUNCTION__, \
                       NEURON_LOG_LABEL, __VA_ARGS__)
#define log_info(...)                                                  \
    neu_log_priv_impl_(NEU_LOG_INFO, __FILE__, __LINE__, __FUNCTION__, \
                       NEURON_LOG_LABEL, __VA_ARGS__)
#define log_warn(...)                                                  \
    neu_log_priv_impl_(NEU_LOG_WARN, __FILE__, __LINE__, __FUNCTION__, \
                       NEURON_LOG_LABEL, __VA_ARGS__)
#define log_error(...)                                                  \
    neu_log_priv_impl_(NEU_LOG_ERROR, __FILE__, __LINE__, __FUNCTION__, \
                       NEURON_LOG_LABEL, __VA_ARGS__)
#define log_fatal(...)                                                  \
    neu_log_priv_impl_(NEU_LOG_FATAL, __FILE__, __LINE__, __FUNCTION__, \
                       NEURON_LOG_LABEL, __VA_ARGS__)

// these will log the node name as the label
#define log_trace_node(plugin, ...)                                     \
    neu_log_priv_impl_(NEU_LOG_TRACE, __FILE__, __LINE__, __FUNCTION__, \
                       neu_plugin_self_node_name(plugin), __VA_ARGS__)
#define log_debug_node(plugin, ...)                                     \
    neu_log_priv_impl_(NEU_LOG_DEBUG, __FILE__, __LINE__, __FUNCTION__, \
                       neu_plugin_self_node_name(plugin), __VA_ARGS__)
#define log_info_node(plugin, ...)                                     \
    neu_log_priv_impl_(NEU_LOG_INFO, __FILE__, __LINE__, __FUNCTION__, \
                       neu_plugin_self_node_name(plugin), __VA_ARGS__)
#define log_warn_node(plugin, ...)                                     \
    neu_log_priv_impl_(NEU_LOG_WARN, __FILE__, __LINE__, __FUNCTION__, \
                       neu_plugin_self_node_name(plugin), __VA_ARGS__)
#define log_error_node(plugin, ...)                                     \
    neu_log_priv_impl_(NEU_LOG_ERROR, __FILE__, __LINE__, __FUNCTION__, \
                       neu_plugin_self_node_name(plugin), __VA_ARGS__)
#define log_fatal_node(plugin, ...)                                     \
    neu_log_priv_impl_(NEU_LOG_FATAL, __FILE__, __LINE__, __FUNCTION__, \
                       neu_plugin_self_node_name(plugin), __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif
