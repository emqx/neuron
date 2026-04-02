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

#include <inttypes.h>

#include "neuron.h"
#include "json/json.h"
#include "json/neu_json_param.h"

#include "kafka_config.h"
#include "kafka_plugin.h"

const char *kafka_security_protocol_str(kafka_security_protocol_e p)
{
    switch (p) {
    case KAFKA_SECURITY_PLAINTEXT:
        return "plaintext";
    case KAFKA_SECURITY_SASL_PLAINTEXT:
        return "sasl_plaintext";
    case KAFKA_SECURITY_SSL:
        return "ssl";
    case KAFKA_SECURITY_SASL_SSL:
        return "sasl_ssl";
    default:
        return "plaintext";
    }
}

const char *kafka_sasl_mechanism_str(kafka_sasl_mechanism_e m)
{
    switch (m) {
    case KAFKA_SASL_PLAIN:
        return "PLAIN";
    case KAFKA_SASL_SCRAM_SHA_256:
        return "SCRAM-SHA-256";
    case KAFKA_SASL_SCRAM_SHA_512:
        return "SCRAM-SHA-512";
    default:
        return "PLAIN";
    }
}

const char *kafka_compression_str(kafka_compression_e c)
{
    switch (c) {
    case KAFKA_COMPRESS_NONE:
        return "none";
    case KAFKA_COMPRESS_GZIP:
        return "gzip";
    case KAFKA_COMPRESS_SNAPPY:
        return "snappy";
    case KAFKA_COMPRESS_LZ4:
        return "lz4";
    case KAFKA_COMPRESS_ZSTD:
        return "zstd";
    default:
        return "none";
    }
}

static int parse_ssl_params(neu_plugin_t *plugin, const char *setting,
                            kafka_config_t *config)
{
    if (config->security_protocol != KAFKA_SECURITY_SSL &&
        config->security_protocol != KAFKA_SECURITY_SASL_SSL) {
        return 0;
    }

    neu_json_elem_t ssl_ca = { .name = "ssl-ca", .t = NEU_JSON_STR };
    int             ret    = neu_parse_param(setting, NULL, 1, &ssl_ca);
    if (0 == ret) {
        int   len = 0;
        char *s   = (char *) neu_decode64(&len, ssl_ca.v.val_str);
        free(ssl_ca.v.val_str);
        if (s && len > 0) {
            config->ssl_ca = s;
        } else {
            free(s);
            plog_notice(plugin, "setting ssl-ca decode fail or empty");
        }
    }

    neu_json_elem_t ssl_cert = { .name = "ssl-cert", .t = NEU_JSON_STR };
    ret                      = neu_parse_param(setting, NULL, 1, &ssl_cert);
    if (0 == ret) {
        int   len = 0;
        char *s   = (char *) neu_decode64(&len, ssl_cert.v.val_str);
        free(ssl_cert.v.val_str);
        if (s && len > 0) {
            config->ssl_cert = s;
        } else {
            free(s);
            plog_notice(plugin, "setting ssl-cert decode fail or empty");
        }
    }

    neu_json_elem_t ssl_key = { .name = "ssl-key", .t = NEU_JSON_STR };
    ret                     = neu_parse_param(setting, NULL, 1, &ssl_key);
    if (0 == ret) {
        int   len = 0;
        char *s   = (char *) neu_decode64(&len, ssl_key.v.val_str);
        free(ssl_key.v.val_str);
        if (s && len > 0) {
            config->ssl_key = s;
        } else {
            free(s);
            plog_notice(plugin, "setting ssl-key decode fail or empty");
        }
    }

    return 0;
}

static int parse_sasl_params(neu_plugin_t *plugin, const char *setting,
                             kafka_config_t *config)
{
    if (config->security_protocol != KAFKA_SECURITY_SASL_PLAINTEXT &&
        config->security_protocol != KAFKA_SECURITY_SASL_SSL) {
        return 0;
    }

    neu_json_elem_t mechanism = {
        .name      = "sasl-mechanism",
        .t         = NEU_JSON_INT,
        .v.val_int = KAFKA_SASL_PLAIN,
        .attribute = NEU_JSON_ATTRIBUTE_OPTIONAL,
    };
    neu_parse_param(setting, NULL, 1, &mechanism);
    config->sasl_mechanism = mechanism.v.val_int;

    neu_json_elem_t username = { .name = "sasl-username", .t = NEU_JSON_STR };
    int             ret      = neu_parse_param(setting, NULL, 1, &username);
    if (0 == ret) {
        config->sasl_username = username.v.val_str;
    } else {
        plog_error(plugin, "setting no sasl-username");
        return -1;
    }

    neu_json_elem_t password = { .name = "sasl-password", .t = NEU_JSON_STR };
    ret                      = neu_parse_param(setting, NULL, 1, &password);
    if (0 == ret) {
        config->sasl_password = password.v.val_str;
    } else {
        plog_error(plugin, "setting no sasl-password");
        return -1;
    }

    return 0;
}

int kafka_config_parse(neu_plugin_t *plugin, const char *setting,
                       kafka_config_t *config)
{
    int         ret         = 0;
    char *      err_param   = NULL;
    const char *placeholder = "********";

    neu_json_elem_t broker = { .name = "broker", .t = NEU_JSON_STR };
    neu_json_elem_t topic  = { .name = "topic", .t = NEU_JSON_STR };
    neu_json_elem_t format = { .name = "format", .t = NEU_JSON_INT };

    neu_json_elem_t upload_err = {
        .name       = "upload_err",
        .t          = NEU_JSON_BOOL,
        .v.val_bool = true,
        .attribute  = NEU_JSON_ATTRIBUTE_OPTIONAL,
    };
    neu_json_elem_t compression = {
        .name      = "compression",
        .t         = NEU_JSON_INT,
        .v.val_int = KAFKA_COMPRESS_NONE,
        .attribute = NEU_JSON_ATTRIBUTE_OPTIONAL,
    };
    neu_json_elem_t batch_max = {
        .name      = "batch-max-messages",
        .t         = NEU_JSON_INT,
        .v.val_int = 10000,
        .attribute = NEU_JSON_ATTRIBUTE_OPTIONAL,
    };
    neu_json_elem_t linger = {
        .name      = "linger-ms",
        .t         = NEU_JSON_INT,
        .v.val_int = 5,
        .attribute = NEU_JSON_ATTRIBUTE_OPTIONAL,
    };
    neu_json_elem_t security = {
        .name      = "security-protocol",
        .t         = NEU_JSON_INT,
        .v.val_int = KAFKA_SECURITY_PLAINTEXT,
        .attribute = NEU_JSON_ATTRIBUTE_OPTIONAL,
    };
    neu_json_elem_t msg_timeout = {
        .name      = "message-timeout-ms",
        .t         = NEU_JSON_INT,
        .v.val_int = 5000,
        .attribute = NEU_JSON_ATTRIBUTE_OPTIONAL,
    };
    neu_json_elem_t acks_param = {
        .name      = "acks",
        .t         = NEU_JSON_INT,
        .v.val_int = -1,
        .attribute = NEU_JSON_ATTRIBUTE_OPTIONAL,
    };
    neu_json_elem_t client_id = {
        .name      = "client-id",
        .t         = NEU_JSON_STR,
        .attribute = NEU_JSON_ATTRIBUTE_OPTIONAL,
    };

    if (NULL == setting || NULL == config) {
        plog_error(plugin, "invalid argument, null pointer");
        return -1;
    }

    ret = neu_parse_param(setting, &err_param, 3, &broker, &topic, &format);
    if (0 != ret) {
        plog_error(plugin, "parsing setting fail, key: `%s`", err_param);
        free(err_param);
        free(broker.v.val_str);
        free(topic.v.val_str);
        return -1;
    }

    config->broker = broker.v.val_str;
    config->topic  = topic.v.val_str;
    config->format = format.v.val_int;

    if (0 == strlen(config->broker)) {
        plog_error(plugin, "setting empty broker");
        goto error;
    }

    if (0 == strlen(config->topic)) {
        plog_error(plugin, "setting empty topic");
        goto error;
    }

    if (config->format != KAFKA_UPLOAD_FORMAT_VALUES &&
        config->format != KAFKA_UPLOAD_FORMAT_TAGS) {
        plog_error(plugin, "setting invalid format: %" PRIi64,
                   (int64_t) config->format);
        goto error;
    }

    neu_parse_param(setting, NULL, 1, &upload_err);
    neu_parse_param(setting, NULL, 1, &compression);
    neu_parse_param(setting, NULL, 1, &batch_max);
    neu_parse_param(setting, NULL, 1, &linger);
    neu_parse_param(setting, NULL, 1, &security);
    neu_parse_param(setting, NULL, 1, &msg_timeout);
    neu_parse_param(setting, NULL, 1, &acks_param);
    neu_parse_param(setting, NULL, 1, &client_id);

    config->upload_err         = upload_err.v.val_bool;
    config->compression        = compression.v.val_int;
    config->batch_max_messages = batch_max.v.val_int;
    config->linger_ms          = linger.v.val_int;
    config->security_protocol  = security.v.val_int;
    config->message_timeout_ms = msg_timeout.v.val_int;
    config->acks               = acks_param.v.val_int;
    config->client_id          = client_id.v.val_str;

    if (config->compression < KAFKA_COMPRESS_NONE ||
        config->compression > KAFKA_COMPRESS_ZSTD) {
        plog_error(plugin, "setting invalid compression: %d",
                   config->compression);
        goto error;
    }

    if (config->batch_max_messages < 1 ||
        config->batch_max_messages > 1000000) {
        plog_error(plugin, "setting invalid batch-max-messages: %d",
                   config->batch_max_messages);
        goto error;
    }

    if (config->linger_ms < 0 || config->linger_ms > 60000) {
        plog_error(plugin, "setting invalid linger-ms: %d", config->linger_ms);
        goto error;
    }

    if (config->security_protocol < KAFKA_SECURITY_PLAINTEXT ||
        config->security_protocol > KAFKA_SECURITY_SASL_SSL) {
        plog_error(plugin, "setting invalid security-protocol: %d",
                   config->security_protocol);
        goto error;
    }

    if (config->message_timeout_ms < 1000 ||
        config->message_timeout_ms > 300000) {
        plog_error(plugin, "setting invalid message-timeout-ms: %d",
                   config->message_timeout_ms);
        goto error;
    }

    if (config->acks != -1 && config->acks != 0 && config->acks != 1) {
        plog_error(plugin, "setting invalid acks: %d", config->acks);
        goto error;
    }

    ret = parse_sasl_params(plugin, setting, config);
    if (0 != ret) {
        goto error;
    }

    ret = parse_ssl_params(plugin, setting, config);
    if (0 != ret) {
        goto error;
    }

    plog_notice(plugin, "config broker             : %s", config->broker);
    plog_notice(plugin, "config topic              : %s", config->topic);
    plog_notice(plugin, "config format             : %d", config->format);
    plog_notice(plugin, "config upload_err         : %d", config->upload_err);
    plog_notice(plugin, "config compression        : %s",
                kafka_compression_str(config->compression));
    plog_notice(plugin, "config batch-max-messages : %d",
                config->batch_max_messages);
    plog_notice(plugin, "config linger-ms          : %d", config->linger_ms);
    plog_notice(plugin, "config security-protocol  : %s",
                kafka_security_protocol_str(config->security_protocol));
    if (config->sasl_username) {
        plog_notice(plugin, "config sasl-username      : %s",
                    config->sasl_username);
    }
    if (config->sasl_password) {
        plog_notice(plugin, "config sasl-password      : %s", placeholder);
    }
    if (config->ssl_ca) {
        plog_notice(plugin, "config ssl-ca             : %s", placeholder);
    }
    plog_notice(plugin, "config message-timeout-ms : %d",
                config->message_timeout_ms);
    plog_notice(plugin, "config acks               : %d", config->acks);
    if (config->client_id) {
        plog_notice(plugin, "config client-id          : %s",
                    config->client_id);
    }

    return 0;

error:
    kafka_config_fini(config);
    return -1;
}

void kafka_config_fini(kafka_config_t *config)
{
    free(config->broker);
    free(config->topic);
    free(config->sasl_username);
    free(config->sasl_password);
    free(config->ssl_ca);
    free(config->ssl_cert);
    free(config->ssl_key);
    free(config->client_id);
    memset(config, 0, sizeof(*config));
}
