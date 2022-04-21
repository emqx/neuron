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

#ifndef ERRORCODES_H
#define ERRORCODES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef int32_t neu_error;

typedef enum {
    NEU_ERR_SUCCESS = 0,

    NEU_ERR_EINTR        = -1,
    NEU_ERR_ENOMEM       = -2,
    NEU_ERR_EINVAL       = -3,
    NEU_ERR_EBUSY        = -4,
    NEU_ERR_ETIMEDOUT    = -5,
    NEU_ERR_ECONNREFUSED = -6,
    NEU_ERR_ECLOSED      = -7,
    NEU_ERR_EAGAIN       = -8,
    NEU_ERR_ENOTSUP      = -9,
    NEU_ERR_EADDRINUSE   = -10,
    NEU_ERR_ESTATE       = -11,
    NEU_ERR_ENOENT       = -12,
    NEU_ERR_EPROTO       = -13,
    NEU_ERR_EUNREACHABLE = -14,
    NEU_ERR_EADDRINVAL   = -15,
    NEU_ERR_EPERM        = -16,
    NEU_ERR_EMSGSIZE     = -17,
    NEU_ERR_ECONNABORTED = -18,
    NEU_ERR_ECONNRESET   = -19,
    NEU_ERR_ECANCELED    = -20,
    NEU_ERR_ENOFILES     = -21,
    NEU_ERR_ENOSPC       = -22,
    NEU_ERR_EEXIST       = -23,
    NEU_ERR_EREADONLY    = -24,
    NEU_ERR_EWRITEONLY   = -25,
    NEU_ERR_ECRYPTO      = -26,
    NEU_ERR_EPEERAUTH    = -27,
    NEU_ERR_ENOARG       = -28,
    NEU_ERR_EAMBIGUOUS   = -29,
    NEU_ERR_EBADTYPE     = -30,
    NEU_ERR_ECONNSHUT    = -31,

    NEU_ERR_FAILURE                  = 1000, // A general error
    NEU_ERR_EINTERNAL                = 1001, // A neuron interanl error
    NEU_ERR_BODY_IS_WRONG            = 1002,
    NEU_ERR_PARAM_IS_WRONG           = 1003,
    NEU_ERR_NEED_TOKEN               = 1004,
    NEU_ERR_DECODE_TOKEN             = 1005,
    NEU_ERR_EXPIRED_TOKEN            = 1006,
    NEU_ERR_VALIDATE_TOKEN           = 1007,
    NEU_ERR_INVALID_TOKEN            = 1008,
    NEU_ERR_INVALID_USER_OR_PASSWORD = 1009,

    NEU_ERR_NODE_TYPE_INVALID      = 2001,
    NEU_ERR_NODE_EXIST             = 2002,
    NEU_ERR_NODE_NOT_EXIST         = 2003,
    NEU_ERR_NODE_SETTING_INVALID   = 2004,
    NEU_ERR_NODE_SETTING_NOT_FOUND = 2005,
    NEU_ERR_NODE_NOT_READY         = 2006,
    NEU_ERR_NODE_IS_RUNNING        = 2007,
    NEU_ERR_NODE_NOT_RUNNING       = 2008,
    NEU_ERR_NODE_IS_STOPED         = 2009,

    NEU_ERR_GRP_CONFIG_NOT_EXIST = 2101,
    NEU_ERR_GRP_CONFIG_IN_USE    = 2102,
    NEU_ERR_GRP_CONFIG_CONFLICT  = 2103,

    NEU_ERR_TAG_NOT_EXIST              = 2201,
    NEU_ERR_TAG_NAME_CONFLICT          = 2202,
    NEU_ERR_TAG_ATTRIBUTE_NOT_SUPPORT  = 2203,
    NEU_ERR_TAG_TYPE_NOT_SUPPORT       = 2204,
    NEU_ERR_TAG_ADDRESS_FORMAT_INVALID = 2205,

    NEU_ERR_LIBRARY_NOT_FOUND     = 2301,
    NEU_ERR_LIBRARY_INFO_INVALID  = 2302,
    NEU_ERR_LIBRARY_NAME_CONFLICT = 2303,

    NEU_ERR_LICENSE_NOT_FOUND = 2400,
    NEU_ERR_LICENSE_INVALID   = 2401,
    NEU_ERR_LICENSE_EXPIRED   = 2402,
    NEU_ERR_LICENSE_DISABLED  = 2403,
    NEU_ERR_LICENSE_MAX_NODES = 2404,
    NEU_ERR_LICENSE_MAX_TAGS  = 2405,

    // common error codes for plugins
    NEU_ERR_PLUGIN_READ_FAILURE        = 3000,
    NEU_ERR_PLUGIN_WRITE_FAILURE       = 3001,
    NEU_ERR_PLUGIN_DISCONNECTED        = 3002,
    NEU_ERR_PLUGIN_TAG_NOT_ALLOW_READ  = 3003,
    NEU_ERR_PLUGIN_TAG_NOT_ALLOW_WRITE = 3004,
    NEU_ERR_PLUGIN_TAG_NOT_EXIST       = 3005,
    NEU_ERR_PLUGIN_GRP_NOT_SUBSCRIBE   = 3006,
    NEU_ERR_PLUGIN_TAG_TYPE_MISMATCH   = 3007,

    NEU_ERR_MQTT_FAILURE                        = 4000,
    NEU_ERR_MQTT_NO_CERTFILESET                 = 4001,
    NEU_ERR_MQTT_CERTFILE_LOAD_FAILURE          = 4002,
    NEU_ERR_MQTT_OPTION_IS_NULL                 = 4003,
    NEU_ERR_MQTT_TOPIC_EMPTY                    = 4004,
    NEU_ERR_MQTT_IS_NULL                        = 4005,
    NEU_ERR_MQTT_INIT_FAILURE                   = 4006,
    NEU_ERR_MQTT_CONNECT_FAILURE                = 4007,
    NEU_ERR_MQTT_SUBSCRIBE_TIMEOUT              = 4008,
    NEU_ERR_MQTT_SUBSCRIBE_LIST_INITIAL_FAILURE = 4009,
    NEU_ERR_MQTT_SUBSCRIBE_FAILURE              = 4010,
    NEU_ERR_MQTT_SUBSCRIBE_ADD_REPEAT           = 4011,
    NEU_ERR_MQTT_SUBSCRIBE_ADD_FAILURE          = 4012,
    NEU_ERR_MQTT_UNSUBSCRIBE_FAILURE            = 4013,
    NEU_ERR_MQTT_PUBLISH_FAILURE                = 4014,
    NEU_ERR_MQTT_SUSPENDED                      = 4015,
    NEU_ERR_MQTT_PUBLISH_OVER_LENGTH            = 4016,

    NEU_ERR_PLUGIN_ERROR_START = 10000,
    NEU_ERR_PLUGIN_ERROR_END   = 20000,
} neu_err_code_e;

#ifdef __cplusplus
}
#endif

#endif /* ERRORCODES_H */
