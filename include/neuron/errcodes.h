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

#ifndef ERRORCODES_H
#define ERRORCODES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef int32_t neu_error;

typedef enum {
    NEU_ERR_SUCCESS = 0,

    NEU_ERR_EINTERNAL                = 1001, // A neuron interanl error
    NEU_ERR_BODY_IS_WRONG            = 1002,
    NEU_ERR_PARAM_IS_WRONG           = 1003,
    NEU_ERR_NEED_TOKEN               = 1004,
    NEU_ERR_DECODE_TOKEN             = 1005,
    NEU_ERR_EXPIRED_TOKEN            = 1006,
    NEU_ERR_VALIDATE_TOKEN           = 1007,
    NEU_ERR_INVALID_TOKEN            = 1008,
    NEU_ERR_INVALID_USER_OR_PASSWORD = 1009,
    NEU_ERR_IS_BUSY                  = 1010,
    NEU_ERR_FILE_NOT_EXIST           = 1011,
    NEU_ERR_INVALID_PASSWORD_LEN     = 1012,
    NEU_ERR_DUPLICATE_PASSWORD       = 1013,
    NEU_ERR_COMMAND_EXECUTION_FAILED = 1014,
    NEU_ERR_IP_ADDRESS_INVALID       = 1015,
    NEU_ERR_IP_ADDRESS_IN_USE        = 1016,
    NEU_ERR_WEAK_PASSWORD_ALPHABET   = 1017,
    NEU_ERR_WEAK_PASSWORD_PATTERN    = 1018,
    NEU_ERR_DECRYPT_PASSWORD_FAILED  = 1019,

    NEU_ERR_NODE_EXIST               = 2002,
    NEU_ERR_NODE_NOT_EXIST           = 2003,
    NEU_ERR_NODE_SETTING_INVALID     = 2004,
    NEU_ERR_NODE_SETTING_NOT_FOUND   = 2005,
    NEU_ERR_NODE_NOT_READY           = 2006,
    NEU_ERR_NODE_IS_RUNNING          = 2007,
    NEU_ERR_NODE_NOT_RUNNING         = 2008,
    NEU_ERR_NODE_IS_STOPED           = 2009,
    NEU_ERR_NODE_NAME_TOO_LONG       = 2010,
    NEU_ERR_NODE_NOT_ALLOW_DELETE    = 2011,
    NEU_ERR_NODE_NOT_ALLOW_SUBSCRIBE = 2012,
    NEU_ERR_NODE_NOT_ALLOW_UPDATE    = 2013,
    NEU_ERR_NODE_NOT_ALLOW_MAP       = 2014,
    NEU_ERR_NODE_NAME_EMPTY          = 2015,

    NEU_ERR_GROUP_ALREADY_SUBSCRIBED = 2101,
    NEU_ERR_GROUP_NOT_SUBSCRIBE      = 2102,
    NEU_ERR_GROUP_NOT_ALLOW          = 2103,
    NEU_ERR_GROUP_EXIST              = 2104,
    NEU_ERR_GROUP_PARAMETER_INVALID  = 2105,
    NEU_ERR_GROUP_NOT_EXIST          = 2106,
    NEU_ERR_GROUP_NAME_TOO_LONG      = 2107,

    NEU_ERR_TAG_NOT_EXIST              = 2201,
    NEU_ERR_TAG_NAME_CONFLICT          = 2202,
    NEU_ERR_TAG_ATTRIBUTE_NOT_SUPPORT  = 2203,
    NEU_ERR_TAG_TYPE_NOT_SUPPORT       = 2204,
    NEU_ERR_TAG_ADDRESS_FORMAT_INVALID = 2205,
    NEU_ERR_TAG_NAME_TOO_LONG          = 2206,
    NEU_ERR_TAG_ADDRESS_TOO_LONG       = 2207,
    NEU_ERR_TAG_DESCRIPTION_TOO_LONG   = 2208,
    NEU_ERR_TAG_PRECISION_INVALID      = 2209,
    NEU_ERR_TAG_EXIST                  = 2210,

    NEU_ERR_LIBRARY_NOT_FOUND                 = 2301,
    NEU_ERR_LIBRARY_INFO_INVALID              = 2302,
    NEU_ERR_LIBRARY_NAME_CONFLICT             = 2303,
    NEU_ERR_LIBRARY_FAILED_TO_OPEN            = 2304,
    NEU_ERR_LIBRARY_MODULE_INVALID            = 2305,
    NEU_ERR_LIBRARY_SYSTEM_NOT_ALLOW_DEL      = 2306,
    NEU_ERR_LIBRARY_NOT_ALLOW_CREATE_INSTANCE = 2307,
    NEU_ERR_LIBRARY_ARCH_NOT_SUPPORT          = 2308,

    NEU_ERR_LICENSE_NOT_FOUND       = 2400,
    NEU_ERR_LICENSE_INVALID         = 2401,
    NEU_ERR_LICENSE_EXPIRED         = 2402,
    NEU_ERR_LICENSE_DISABLED        = 2403,
    NEU_ERR_LICENSE_MAX_NODES       = 2404,
    NEU_ERR_LICENSE_MAX_TAGS        = 2405,
    NEU_ERR_LICENSE_TOKEN_NOT_MATCH = 2406,
    NEU_ERR_LICENSE_BAD_CLOCK       = 2407,
    NEU_ERR_LICENSE_MODULE_INVALID  = 2408,
    NEU_ERR_LICENSE_TOKEN_NOT_FOUND = 2409,

    NEU_ERR_TEMPLATE_EXIST         = 2500,
    NEU_ERR_TEMPLATE_NOT_FOUND     = 2501,
    NEU_ERR_TEMPLATE_NAME_TOO_LONG = 2502,

    // common error codes for plugins
    NEU_ERR_PLUGIN_READ_FAILURE            = 3000,
    NEU_ERR_PLUGIN_WRITE_FAILURE           = 3001,
    NEU_ERR_PLUGIN_DISCONNECTED            = 3002,
    NEU_ERR_PLUGIN_TAG_NOT_ALLOW_READ      = 3003,
    NEU_ERR_PLUGIN_TAG_NOT_ALLOW_WRITE     = 3004,
    NEU_ERR_PLUGIN_TAG_TYPE_MISMATCH       = 3007,
    NEU_ERR_PLUGIN_TAG_VALUE_EXPIRED       = 3008,
    NEU_ERR_PLUGIN_PROTOCOL_DECODE_FAILURE = 3009,
    NEU_ERR_PLUGIN_NOT_RUNNING             = 3010,
    NEU_ERR_PLUGIN_TAG_NOT_READY           = 3011,
    NEU_ERR_PLUGIN_PACKET_OUT_OF_ORDER     = 3012,
    NEU_ERR_PLUGIN_NAME_TOO_LONG           = 3013,
    NEU_ERR_PLUGIN_NOT_FOUND               = 3014,
    NEU_ERR_PLUGIN_DEVICE_NOT_RESPONSE     = 3015,
    NEU_ERR_PLUGIN_NOT_SUPPORT_TEMPLATE    = 3016,
    NEU_ERR_PLUGIN_NOT_SUPPORT_WRITE_TAGS  = 3017,
    NEU_ERR_PLUGIN_NOT_SUPPORT_READ_SYNC   = 3018,

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

    NEU_ERR_STRING_TOO_LONG    = 4100,
    NEU_ERR_FILE_OPEN_FAILURE  = 4101,
    NEU_ERR_FILE_READ_FAILURE  = 4102,
    NEU_ERR_FILE_WRITE_FAILURE = 4103,

    NEU_ERR_PLUGIN_ERROR_START = 10000,
    NEU_ERR_PLUGIN_ERROR_END   = 20000,
} neu_err_code_e;

#ifdef __cplusplus
}
#endif

#endif /* ERRORCODES_H */
