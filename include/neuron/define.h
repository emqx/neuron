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

#ifndef _NEU_DEFINE_H_
#define _NEU_DEFINE_H_

#include <stdbool.h>

#define NEU_VERSION_MAJOR 2
#define NEU_VERSION_MINOR 8
#define NEU_VERSION_FIX 3

#define NEU_GET_VERSION_MAJOR(V) ((0xffff0000 & V) >> 16)
#define NEU_GET_VERSION_MINOR(V) ((0x0000ff00 & V) >> 8)
#define NEU_GET_VERSION_FIX(V) ((0x000000ff & V))
#define NEU_VERSION(MAJOR, MINOR, FIX) ((MAJOR << 16) | (MINOR << 8) | FIX)

#define NEU_TAG_NAME_LEN 128
#define NEU_TAG_ADDRESS_LEN 128
#define NEU_TAG_DESCRIPTION_LEN 128
#define NEU_GROUP_MAX_PER_NODE 512
#define NEU_GROUP_NAME_LEN 128
#define NEU_GROUP_INTERVAL_LIMIT 100
#define NEU_DEFAULT_GROUP_INTERVAL 100
#define NEU_NODE_NAME_LEN 128
#define NEU_PLUGIN_NAME_LEN 32
#define NEU_PLUGIN_LIBRARY_LEN 64
#define NEU_PLUGIN_DESCRIPTION_LEN 512
#define NEU_TEMPLATE_NAME_LEN 128
#define NEU_DRIVER_TAG_CACHE_EXPIRE_TIME 60
#define NEU_APP_SUBSCRIBE_MSG_SIZE 4
#define NEU_TAG_FLOAG_PRECISION_MAX 17
#define NEU_USER_PASSWORD_MIN_LEN 4
#define NEU_USER_PASSWORD_MAX_LEN 16
#define NEU_LOG_LEVEL_LEN 9
#define NEU_FILE_PATH_LEN 128
#define NEU_MSG_MAX_SIZE 2048

#define NEU_TAG_META_LENGTH 20
#define NEU_TAG_META_SIZE 3

#define NEU_LOG_LEVEL_DEBUG "debug"
#define NEU_LOG_LEVEL_INFO "info"
#define NEU_LOG_LEVEL_NOTICE "notice"
#define NEU_LOG_LEVEL_WARN "warn"
#define NEU_LOG_LEVEL_ERROR "error"
#define NEU_LOG_LEVEL_FATAL "fatal"

#define CHECK_NODE_NAME_LENGTH_ERR                                            \
    do {                                                                      \
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_NODE_NAME_TOO_LONG, {                 \
            neu_http_response(aio, NEU_ERR_NODE_NAME_TOO_LONG, result_error); \
        });                                                                   \
    } while (0)

#define CHECK_GROUP_NAME_LENGTH_ERR                                            \
    do {                                                                       \
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_GROUP_NAME_TOO_LONG, {                 \
            neu_http_response(aio, NEU_ERR_GROUP_NAME_TOO_LONG, result_error); \
        });                                                                    \
    } while (0)

#define CHECK_TAG_NAME_LENGTH_ERR                                            \
    do {                                                                     \
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_TAG_NAME_TOO_LONG, {                 \
            neu_http_response(aio, NEU_ERR_TAG_NAME_TOO_LONG, result_error); \
        });                                                                  \
    } while (0)

#define CHECK_GROUP_INTERVAL_ERR                                    \
    do {                                                            \
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_GROUP_PARAMETER_INVALID, {  \
            neu_http_response(aio, NEU_ERR_GROUP_PARAMETER_INVALID, \
                              result_error);                        \
        });                                                         \
    } while (0)

extern int  default_log_level;
extern bool disable_jwt;
extern char host_port[32];

typedef enum neu_plugin_kind {
    NEU_PLUGIN_KIND_STATIC = 0,
    NEU_PLUGIN_KIND_SYSTEM = 1,
    NEU_PLUGIN_KIND_CUSTOM = 2,
} neu_plugin_kind_e;

typedef enum {
    NEU_NA_TYPE_DRIVER = 1,
    NEU_NA_TYPE_APP    = 2,
} neu_adapter_type_e,
    neu_node_type_e;

typedef enum {
    NEU_NODE_LINK_STATE_DISCONNECTED = 0,
    NEU_NODE_LINK_STATE_CONNECTED    = 1,
} neu_node_link_state_e;

typedef enum {
    NEU_NODE_RUNNING_STATE_INIT    = 1,
    NEU_NODE_RUNNING_STATE_READY   = 2,
    NEU_NODE_RUNNING_STATE_RUNNING = 3,
    NEU_NODE_RUNNING_STATE_STOPPED = 4,
} neu_node_running_state_e;

typedef enum {
    NEU_TAG_CACHE_TYPE_INTERVAL = 0,
    NEU_TAG_CACHE_TYPE_NEVER,
} neu_tag_cache_type_e;

// forward declaration for neu_adapter_t
typedef struct neu_adapter neu_adapter_t;

#endif
