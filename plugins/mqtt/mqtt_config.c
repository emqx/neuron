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

#include "json/json.h"
#include "json/neu_json_param.h"

#include "mqtt_config.h"
#include "mqtt_plugin.h"

#define MB 1000000

static inline int decode_b64_param(neu_plugin_t *plugin, neu_json_elem_t *el)
{
    int   len = 0;
    char *s   = (char *) neu_decode64(&len, el->v.val_str);

    if (NULL == s) {
        plog_error(plugin, "setting %s invalid base64", el->name);
        return -1;
    }

    if (0 == len) {
        plog_error(plugin, "setting empty %s", el->name);
        free(s);
        return -1;
    }

    free(el->v.val_str);
    el->v.val_str = s;
    return 0;
}

static inline int parse_b64_param(neu_plugin_t *plugin, const char *setting,
                                  neu_json_elem_t *el)
{
    if (0 != neu_parse_param(setting, NULL, 1, el)) {
        plog_error(plugin, "setting no %s", el->name);
        return -1;
    }

    if (0 != decode_b64_param(plugin, el)) {
        free(el->v.val_str);
        el->v.val_str = NULL;
        return -1;
    }

    return 0;
}

static inline int parse_ssl_params(neu_plugin_t *plugin, const char *setting,
                                   neu_json_elem_t *ssl, neu_json_elem_t *ca,
                                   neu_json_elem_t *cert, neu_json_elem_t *key,
                                   neu_json_elem_t *keypass)
{
    // ssl, optional
    int ret = neu_parse_param(setting, NULL, 1, ssl);
    if (0 != ret) {
        plog_info(plugin, "setting no ssl");
        return 0;
    }

    if (false == ssl->v.val_bool) {
        plog_info(plugin, "setting ssl disabled");
        return 0;
    }

    // ca, required if ssl enabled
    ret = parse_b64_param(plugin, setting, ca);
    if (0 != ret) {
        return -1;
    }

    // cert, optional
    ret = neu_parse_param(setting, NULL, 1, cert);
    if (0 != ret) {
        plog_info(plugin, "setting no cert");
        return 0;
    }

    if (0 != decode_b64_param(plugin, cert)) {
        return -1;
    }

    // key, required if cert enable
    ret = parse_b64_param(plugin, setting, key);
    if (0 != ret) {
        return -1;
    }

    // keypass, optional
    ret = neu_parse_param(setting, NULL, 1, keypass);
    if (0 != ret) {
        plog_info(plugin, "setting no keypass");
    } else if (0 == strlen(keypass->v.val_str)) {
        plog_error(plugin, "setting invalid keypass");
        return -1;
    }

    return 0;
}

int mqtt_config_parse(neu_plugin_t *plugin, const char *setting,
                      mqtt_config_t *config)
{
    int         ret         = 0;
    char *      err_param   = NULL;
    const char *placeholder = "********";

    neu_json_elem_t client_id = { .name = "client-id", .t = NEU_JSON_STR };
    neu_json_elem_t qos       = {
        .name      = "qos",
        .t         = NEU_JSON_INT,
        .v.val_int = NEU_MQTT_QOS0,               // default to QoS0
        .attribute = NEU_JSON_ATTRIBUTE_OPTIONAL, // for backward compatibility
    };
    neu_json_elem_t format          = { .name = "format", .t = NEU_JSON_INT };
    neu_json_elem_t cache_mem_size  = { .name = "cache-mem-size",
                                       .t    = NEU_JSON_INT };
    neu_json_elem_t cache_disk_size = { .name = "cache-disk-size",
                                        .t    = NEU_JSON_INT };
    neu_json_elem_t host            = { .name = "host", .t = NEU_JSON_STR };
    neu_json_elem_t port            = { .name = "port", .t = NEU_JSON_INT };
    neu_json_elem_t username        = { .name = "username", .t = NEU_JSON_STR };
    neu_json_elem_t password        = { .name = "password", .t = NEU_JSON_STR };
    neu_json_elem_t ssl             = { .name = "ssl", .t = NEU_JSON_BOOL };
    neu_json_elem_t ca              = { .name = "ca", .t = NEU_JSON_STR };
    neu_json_elem_t cert            = { .name = "cert", .t = NEU_JSON_STR };
    neu_json_elem_t key             = { .name = "key", .t = NEU_JSON_STR };
    neu_json_elem_t keypass         = { .name = "keypass", .t = NEU_JSON_STR };

    if (NULL == setting || NULL == config) {
        plog_error(plugin, "invalid argument, null pointer");
        return -1;
    }

    ret = neu_parse_param(setting, &err_param, 7, &client_id, &format, &qos,
                          &cache_mem_size, &cache_disk_size, &host, &port);
    if (0 != ret) {
        plog_error(plugin, "parsing setting fail, key: `%s`", err_param);
        goto error;
    }

    // client-id, required
    if (0 == strlen(client_id.v.val_str)) {
        plog_error(plugin, "setting empty client-id");
        goto error;
    }

    // qos, optional, default to QoS0
    if (qos.v.val_int < NEU_MQTT_QOS0 || NEU_MQTT_QOS2 < qos.v.val_int) {
        plog_error(plugin, "setting invalid qos: %" PRIi64, qos.v.val_int);
        goto error;
    }

    // format, required
    if (MQTT_UPLOAD_FORMAT_VALUES != format.v.val_int &&
        MQTT_UPLOAD_FORMAT_TAGS != format.v.val_int) {
        plog_error(plugin, "setting invalid format: %" PRIi64,
                   format.v.val_int);
        goto error;
    }

    // cache memory size, required
    if (cache_mem_size.v.val_int > 1024) {
        plog_error(plugin, "setting invalid cache memory size: %" PRIi64,
                   cache_mem_size.v.val_int);
        goto error;
    }

    // cache disk size, required
    if (cache_disk_size.v.val_int > 10240) {
        plog_error(plugin, "setting invalid cache disk size: %" PRIi64,
                   cache_disk_size.v.val_int);
        goto error;
    }

    if (cache_mem_size.v.val_int > cache_disk_size.v.val_int) {
        plog_error(plugin,
                   "setting cache memory size %" PRIi64
                   " larger than cache disk size %" PRIi64,
                   cache_mem_size.v.val_int, cache_disk_size.v.val_int);
        goto error;
    }

    if (0 == cache_mem_size.v.val_int && 0 != cache_disk_size.v.val_int) {
        plog_error(plugin,
                   "setting cache disk size %" PRIi64 " without memory cache",
                   cache_disk_size.v.val_int);
        goto error;
    }

    // host, required
    if (0 == strlen(host.v.val_str)) {
        plog_error(plugin, "setting invalid host: `%s`", host.v.val_str);
        goto error;
    }

    // port, required
    if (0 == port.v.val_int || port.v.val_int > 65535) {
        plog_error(plugin, "setting invalid port: %" PRIi64, port.v.val_int);
        goto error;
    }

    // username, optional
    ret = neu_parse_param(setting, NULL, 1, &username);
    if (0 != ret) {
        plog_info(plugin, "setting no username");
    }

    // password, optional
    ret = neu_parse_param(setting, NULL, 1, &password);
    if (0 != ret) {
        plog_info(plugin, "setting no password");
    }

    ret = parse_ssl_params(plugin, setting, &ssl, &ca, &cert, &key, &keypass);
    if (0 != ret) {
        goto error;
    }

    config->client_id = client_id.v.val_str;
    config->qos       = qos.v.val_int;
    config->format    = format.v.val_int;
    config->cache =
        0 != cache_mem_size.v.val_int || 0 != cache_disk_size.v.val_int;
    config->cache_mem_size  = cache_mem_size.v.val_int * MB;
    config->cache_disk_size = cache_disk_size.v.val_int * MB;
    config->host            = host.v.val_str;
    config->port            = port.v.val_int;
    config->username        = username.v.val_str;
    config->password        = password.v.val_str;
    config->ca              = ca.v.val_str;
    config->cert            = cert.v.val_str;
    config->key             = key.v.val_str;
    config->keypass         = keypass.v.val_str;

    plog_info(plugin, "config client-id      : %s", config->client_id);
    plog_info(plugin, "config qos            : %d", config->qos);
    plog_info(plugin, "config format         : %s",
              mqtt_upload_format_str(config->format));
    plog_info(plugin, "config cache          : %zu", config->cache);
    plog_info(plugin, "config cache-mem-size : %zu", config->cache_mem_size);
    plog_info(plugin, "config cache-disk-size: %zu", config->cache_disk_size);
    plog_info(plugin, "config host           : %s", config->host);
    plog_info(plugin, "config port           : %" PRIu16, config->port);

    if (config->username) {
        plog_info(plugin, "config username       : %s", config->username);
    }
    if (config->password) {
        plog_info(plugin, "config password       : %s",
                  0 == strlen(config->password) ? "" : placeholder);
    }
    if (config->ca) {
        plog_info(plugin, "config ca             : %s", placeholder);
    }
    if (config->cert) {
        plog_info(plugin, "config cert           : %s", placeholder);
    }
    if (config->key) {
        plog_info(plugin, "config key            : %s", placeholder);
    }
    if (config->keypass) {
        plog_info(plugin, "config keypass        : %s", placeholder);
    }

    return 0;

error:
    free(err_param);
    free(host.v.val_str);
    free(username.v.val_str);
    free(password.v.val_str);
    free(ca.v.val_str);
    free(cert.v.val_str);
    free(key.v.val_str);
    free(keypass.v.val_str);
    return -1;
}

void mqtt_config_fini(mqtt_config_t *config)
{
    free(config->client_id);
    free(config->host);
    free(config->username);
    free(config->password);
    free(config->ca);
    free(config->cert);
    free(config->key);
    free(config->keypass);

    memset(config, 0, sizeof(*config));
}
