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

#ifndef NEURON_PLUGIN_KAFKA_CONFIG_H
#define NEURON_PLUGIN_KAFKA_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>

#include "plugin.h"

typedef enum {
    KAFKA_UPLOAD_FORMAT_VALUES = 0,
    KAFKA_UPLOAD_FORMAT_TAGS   = 1,
} kafka_upload_format_e;

typedef enum {
    KAFKA_COMPRESS_NONE   = 0,
    KAFKA_COMPRESS_GZIP   = 1,
    KAFKA_COMPRESS_SNAPPY = 2,
    KAFKA_COMPRESS_LZ4    = 3,
    KAFKA_COMPRESS_ZSTD   = 4,
} kafka_compression_e;

typedef enum {
    KAFKA_SECURITY_PLAINTEXT      = 0,
    KAFKA_SECURITY_SASL_PLAINTEXT = 1,
    KAFKA_SECURITY_SSL            = 2,
    KAFKA_SECURITY_SASL_SSL       = 3,
} kafka_security_protocol_e;

typedef enum {
    KAFKA_SASL_PLAIN         = 0,
    KAFKA_SASL_SCRAM_SHA_256 = 1,
    KAFKA_SASL_SCRAM_SHA_512 = 2,
} kafka_sasl_mechanism_e;

typedef struct {
    char *broker;
    char *topic;

    kafka_upload_format_e     format;
    bool                      upload_err;
    kafka_compression_e       compression;
    int                       batch_max_messages;
    int                       linger_ms;
    kafka_security_protocol_e security_protocol;
    kafka_sasl_mechanism_e    sasl_mechanism;
    char *                    sasl_username;
    char *                    sasl_password;
    char *                    ssl_ca;
    char *                    ssl_cert;
    char *                    ssl_key;

    int   message_timeout_ms;
    int   acks;
    char *client_id;
} kafka_config_t;

int  kafka_config_parse(neu_plugin_t *plugin, const char *setting,
                        kafka_config_t *config);
void kafka_config_fini(kafka_config_t *config);

const char *kafka_security_protocol_str(kafka_security_protocol_e p);
const char *kafka_sasl_mechanism_str(kafka_sasl_mechanism_e m);
const char *kafka_compression_str(kafka_compression_e c);

#ifdef __cplusplus
}
#endif

#endif
