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

#include "utils/asprintf.h"
#include "json/json.h"
#include "json/neu_json_param.h"

#include "mqtt_config.h"
#include "mqtt_plugin.h"

#define MB 1000000

int decode_b64_param(neu_plugin_t *plugin, neu_json_elem_t *el)
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

int parse_b64_param(neu_plugin_t *plugin, const char *setting,
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

static int parse_ssl_params(neu_plugin_t *plugin, const char *setting,
                            neu_json_elem_t *ssl, neu_json_elem_t *ca,
                            neu_json_elem_t *cert, neu_json_elem_t *key,
                            neu_json_elem_t *keypass)
{
    // ssl, optional
    int ret = neu_parse_param(setting, NULL, 1, ssl);
    if (0 != ret) {
        plog_notice(plugin, "setting no ssl");
        return 0;
    }

    if (false == ssl->v.val_bool) {
        plog_notice(plugin, "setting ssl disabled");
        return 0;
    }

    // ca, optional
    ret = neu_parse_param(setting, NULL, 1, ca);
    if (0 != ret) {
        plog_notice(plugin, "setting no ca");
        return 0;
    }

    if (0 != decode_b64_param(plugin, ca)) {
        return -1;
    }

    // cert, optional
    ret = neu_parse_param(setting, NULL, 1, cert);
    if (0 != ret) {
        plog_notice(plugin, "setting no cert");
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
    // keep it here for backward compatibility with version 2.5
    ret = neu_parse_param(setting, NULL, 1, keypass);
    if (0 != ret) {
        plog_notice(plugin, "setting no keypass");
    } else if (0 == strlen(keypass->v.val_str)) {
        plog_error(plugin, "setting invalid keypass");
        return -1;
    }

    return 0;
}

static int parse_heartbeat_params(neu_plugin_t *plugin, const char *setting,
                                  neu_json_elem_t *upload_drv_state,
                                  neu_json_elem_t *upload_drv_state_topic,
                                  neu_json_elem_t *upload_drv_state_interval)
{
    int ret = neu_parse_param(setting, NULL, 1, upload_drv_state);
    if (0 != ret) {
        plog_notice(plugin, "setting upload_drv_state failed");
        return -1;
    }

    if (false == upload_drv_state->v.val_bool) {
        plog_notice(plugin, "setting upload_drv_state disabled");
        return 0;
    }

    ret = neu_parse_param(setting, NULL, 1, upload_drv_state_topic);
    if (0 != ret) {
        plog_error(plugin, "setting no upload_drv_state_topic");
        return -1;
    }

    ret = neu_parse_param(setting, NULL, 1, upload_drv_state_interval);
    if (0 != ret) {
        plog_error(plugin, "setting no upload_drv_state_interval");
        return -1;
    }

    return 0;
}

static int parse_cache_params(neu_plugin_t *plugin, const char *setting,
                              neu_json_elem_t *offline_cache,
                              neu_json_elem_t *cache_mem_size,
                              neu_json_elem_t *cache_disk_size,
                              neu_json_elem_t *cache_sync_interval)
{
    int   ret          = 0;
    char *err_param    = NULL;
    bool  flag_present = true; // whether setting has `offline-cache`

    // offline-cache flag, optional
    ret = neu_parse_param(setting, NULL, 1, offline_cache);
    if (0 != ret) {
        plog_notice(plugin, "setting no offline cache flag");
        flag_present = false; // `offline-cache` not present in setting
    }

    if (flag_present && !offline_cache->v.val_bool) {
        // cache explicitly disabled in setting
        cache_mem_size->v.val_int      = 0;
        cache_disk_size->v.val_int     = 0;
        cache_sync_interval->v.val_int = NEU_MQTT_CACHE_SYNC_INTERVAL_DEFAULT;
        return 0;
    }

    // we are here because one of the following:
    //    1. `offline-cache` presents to be true in setting
    //    2. no `offline-cache` in setting
    // we require `cache_mem_size` and `cache_disk_size` to present in both
    // cases, especially in case 2 for backward compatibility.

    ret = neu_parse_param(setting, &err_param, 2, cache_mem_size,
                          cache_disk_size);
    if (0 != ret) {
        plog_error(plugin, "parsing setting fail, key: `%s`", err_param);
        free(err_param);
        return -1;
    }

    if (cache_mem_size->v.val_int > 1024 ||
        (flag_present && cache_mem_size->v.val_int < 1)) {
        plog_error(plugin, "setting invalid cache memory size: %" PRIi64,
                   cache_mem_size->v.val_int);
        return -1;
    }

    if (cache_disk_size->v.val_int > 10240 ||
        (flag_present && cache_disk_size->v.val_int < 1)) {
        plog_error(plugin, "setting invalid cache disk size: %" PRIi64,
                   cache_disk_size->v.val_int);
        return -1;
    }

    if (cache_mem_size->v.val_int > cache_disk_size->v.val_int) {
        plog_error(plugin,
                   "setting cache memory size %" PRIi64
                   " larger than cache disk size %" PRIi64,
                   cache_mem_size->v.val_int, cache_disk_size->v.val_int);
        return -1;
    }

    if (0 == cache_mem_size->v.val_int && 0 != cache_disk_size->v.val_int) {
        plog_error(plugin,
                   "setting cache disk size %" PRIi64 " without memory cache",
                   cache_disk_size->v.val_int);
        return -1;
    }

    if (!flag_present) {
        offline_cache->v.val_bool = cache_mem_size->v.val_int > 0;
    }

    // cache-sync-interval, optional for backward compatibility
    ret = neu_parse_param(setting, NULL, 1, cache_sync_interval);
    if (0 == ret) {
        if (cache_sync_interval->v.val_int < NEU_MQTT_CACHE_SYNC_INTERVAL_MIN ||
            NEU_MQTT_CACHE_SYNC_INTERVAL_MAX < cache_sync_interval->v.val_int) {
            plog_error(plugin, "setting invalid cache sync interval: %" PRIi64,
                       cache_sync_interval->v.val_int);
            return -1;
        }
    } else {
        plog_notice(plugin, "setting no cache sync interval");
        cache_sync_interval->v.val_int = NEU_MQTT_CACHE_SYNC_INTERVAL_DEFAULT;
    }

    return 0;
}

int mqtt_config_parse(neu_plugin_t *plugin, const char *setting,
                      mqtt_config_t *config)
{
    int         ret         = 0;
    char *      err_param   = NULL;
    const char *placeholder = "********";

    neu_json_elem_t version = {
        .name      = "version",
        .t         = NEU_JSON_INT,
        .v.val_int = NEU_MQTT_VERSION_V311,       // default to V311
        .attribute = NEU_JSON_ATTRIBUTE_OPTIONAL, // for backward compatibility
    };
    neu_json_elem_t client_id = { .name = "client-id", .t = NEU_JSON_STR };
    neu_json_elem_t qos       = {
        .name      = "qos",
        .t         = NEU_JSON_INT,
        .v.val_int = NEU_MQTT_QOS0,               // default to QoS0
        .attribute = NEU_JSON_ATTRIBUTE_OPTIONAL, // for backward compatibility
    };
    neu_json_elem_t format          = { .name = "format", .t = NEU_JSON_INT };
    neu_json_elem_t write_req_topic = {
        .name      = "write-req-topic",
        .t         = NEU_JSON_STR,
        .v.val_str = NULL,
        .attribute = NEU_JSON_ATTRIBUTE_OPTIONAL, // for backward compatibility
    };
    neu_json_elem_t write_resp_topic = {
        .name      = "write-resp-topic",
        .t         = NEU_JSON_STR,
        .v.val_str = NULL,
        .attribute = NEU_JSON_ATTRIBUTE_OPTIONAL, // for backward compatibility
    };
    neu_json_elem_t driver_action_req_topic = {
        .name      = "driver-action-req-topic",
        .t         = NEU_JSON_STR,
        .v.val_str = NULL,
        .attribute = NEU_JSON_ATTRIBUTE_OPTIONAL,
    };
    neu_json_elem_t driver_action_resp_topic = {
        .name      = "driver-action-resp-topic",
        .t         = NEU_JSON_STR,
        .v.val_str = NULL,
        .attribute = NEU_JSON_ATTRIBUTE_OPTIONAL,
    };
    neu_json_elem_t offline_cache       = { .name = "offline-cache",
                                      .t    = NEU_JSON_BOOL };
    neu_json_elem_t cache_mem_size      = { .name = "cache-mem-size",
                                       .t    = NEU_JSON_INT };
    neu_json_elem_t cache_disk_size     = { .name = "cache-disk-size",
                                        .t    = NEU_JSON_INT };
    neu_json_elem_t cache_sync_interval = { .name = "cache-sync-interval",
                                            .t    = NEU_JSON_INT };
    neu_json_elem_t host                = { .name = "host", .t = NEU_JSON_STR };
    neu_json_elem_t port                = { .name = "port", .t = NEU_JSON_INT };
    neu_json_elem_t username = { .name = "username", .t = NEU_JSON_STR };
    neu_json_elem_t password = { .name = "password", .t = NEU_JSON_STR };
    neu_json_elem_t ssl      = { .name = "ssl", .t = NEU_JSON_BOOL };
    neu_json_elem_t ca       = { .name = "ca", .t = NEU_JSON_STR };
    neu_json_elem_t cert     = { .name = "cert", .t = NEU_JSON_STR };
    neu_json_elem_t key      = { .name = "key", .t = NEU_JSON_STR };
    neu_json_elem_t keypass  = { .name = "keypass", .t = NEU_JSON_STR };
    neu_json_elem_t upload_drv_state = {
        .name       = "upload_drv_state",
        .t          = NEU_JSON_BOOL,
        .v.val_bool = false,
        .attribute  = NEU_JSON_ATTRIBUTE_OPTIONAL,
    };
    neu_json_elem_t upload_drv_state_topic = { .name = "upload_drv_state_topic",
                                               .t    = NEU_JSON_STR };
    neu_json_elem_t upload_drv_state_interval = {
        .name = "upload_drv_state_interval", .t = NEU_JSON_INT
    };

    if (NULL == setting || NULL == config) {
        plog_error(plugin, "invalid argument, null pointer");
        return -1;
    }

    ret = neu_parse_param(setting, &err_param, 7, &client_id, &qos, &format,
                          &write_req_topic, &write_resp_topic, &host, &port);
    if (0 != ret) {
        plog_error(plugin, "parsing setting fail, key: `%s`", err_param);
        goto error;
    }

    ret = neu_parse_param(setting, &err_param, 1, &version);
    if (0 != ret) {
        plog_error(plugin, "parsing mqtt version fail, key: `%s`.", err_param);
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

    ret = neu_parse_param(setting, &err_param, 2, &driver_action_req_topic,
                          &driver_action_resp_topic);
    if (0 != ret) {
        plog_warn(plugin, "parsing action topic fail, key: `%s`", err_param);
    }

    // write request topic
    if (NULL == write_req_topic.v.val_str &&
        0 > neu_asprintf(&write_req_topic.v.val_str, "/neuron/%s/write/req",
                         plugin->common.name)) {
        plog_error(plugin, "setting write request topic error");
        goto error;
    }

    // write response topic
    if (NULL == write_resp_topic.v.val_str &&
        0 > neu_asprintf(&write_resp_topic.v.val_str, "/neuron/%s/write/resp",
                         plugin->common.name)) {
        plog_error(plugin, "setting write response topic error");
        goto error;
    }

    // driver action request topic
    if (NULL == driver_action_req_topic.v.val_str &&
        0 > neu_asprintf(&driver_action_req_topic.v.val_str,
                         "/neuron/%s/action/req", plugin->common.name)) {
        plog_error(plugin, "setting driver action request topic error");
        goto error;
    }

    // driver action response topic
    if (NULL == driver_action_resp_topic.v.val_str &&
        0 > neu_asprintf(&driver_action_resp_topic.v.val_str,
                         "/neuron/%s/action/resp", plugin->common.name)) {
        plog_error(plugin, "setting driver action response topic error");
        goto error;
    }

    // offline cache
    ret = parse_cache_params(plugin, setting, &offline_cache, &cache_mem_size,
                             &cache_disk_size, &cache_sync_interval);
    if (0 != ret) {
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
        plog_notice(plugin, "setting no username");
    }

    // password, optional
    ret = neu_parse_param(setting, NULL, 1, &password);
    if (0 != ret) {
        plog_notice(plugin, "setting no password");
    }

    ret = parse_ssl_params(plugin, setting, &ssl, &ca, &cert, &key, &keypass);
    if (0 != ret) {
        goto error;
    }

    ret = parse_heartbeat_params(plugin, setting, &upload_drv_state,
                                 &upload_drv_state_topic,
                                 &upload_drv_state_interval);
    if (0 != ret) {
        goto error;
    }

    config->version                  = version.v.val_int;
    config->client_id                = client_id.v.val_str;
    config->qos                      = qos.v.val_int;
    config->format                   = format.v.val_int;
    config->write_req_topic          = write_req_topic.v.val_str;
    config->write_resp_topic         = write_resp_topic.v.val_str;
    config->driver_action_req_topic  = driver_action_req_topic.v.val_str;
    config->driver_action_resp_topic = driver_action_resp_topic.v.val_str;
    config->cache                    = offline_cache.v.val_bool;
    config->cache_mem_size           = cache_mem_size.v.val_int * MB;
    config->cache_disk_size          = cache_disk_size.v.val_int * MB;
    config->cache_sync_interval      = cache_sync_interval.v.val_int;
    config->host                     = host.v.val_str;
    config->port                     = port.v.val_int;
    config->username                 = username.v.val_str;
    config->password                 = password.v.val_str;
    config->ssl                      = ssl.v.val_bool;
    config->ca                       = ca.v.val_str;
    config->cert                     = cert.v.val_str;
    config->key                      = key.v.val_str;
    config->keypass                  = keypass.v.val_str;
    config->upload_drv_state         = upload_drv_state.v.val_bool;
    config->heartbeat_topic          = upload_drv_state_topic.v.val_str;
    config->heartbeat_interval       = upload_drv_state_interval.v.val_int;

    plog_notice(plugin, "config MQTT version    : %d", config->version);
    plog_notice(plugin, "config client-id       : %s", config->client_id);
    plog_notice(plugin, "config qos             : %d", config->qos);
    plog_notice(plugin, "config format          : %s",
                mqtt_upload_format_str(config->format));
    plog_notice(plugin, "config write-req-topic : %s", config->write_req_topic);
    plog_notice(plugin, "config write-resp-topic: %s",
                config->write_resp_topic);
    plog_notice(plugin, "config driver-action-req-topic : %s",
                config->driver_action_req_topic);
    plog_notice(plugin, "config driver-action-resp-topic: %s",
                config->driver_action_resp_topic);
    plog_notice(plugin, "config upload-drv-state: %d",
                config->upload_drv_state);
    if (config->upload_drv_state) {
        if (config->heartbeat_topic) {
            plog_notice(plugin, "config upload-drv-state-topic: %s",
                        config->heartbeat_topic);
        }
        plog_notice(plugin, "config upload-drv-state-interval: %d",
                    config->heartbeat_interval);
    }
    plog_notice(plugin, "config cache           : %zu", config->cache);
    plog_notice(plugin, "config cache-mem-size  : %zu", config->cache_mem_size);
    plog_notice(plugin, "config cache-disk-size : %zu",
                config->cache_disk_size);
    plog_notice(plugin, "config cache-sync-interval : %zu",
                config->cache_sync_interval);
    plog_notice(plugin, "config host            : %s", config->host);
    plog_notice(plugin, "config port            : %" PRIu16, config->port);

    if (config->username) {
        plog_notice(plugin, "config username        : %s", config->username);
    }
    if (config->password) {
        plog_notice(plugin, "config password        : %s",
                    0 == strlen(config->password) ? "" : placeholder);
    }

    plog_notice(plugin, "config ssl             : %d", config->ssl);
    if (config->ca) {
        plog_notice(plugin, "config ca              : %s", placeholder);
    }
    if (config->cert) {
        plog_notice(plugin, "config cert            : %s", placeholder);
    }
    if (config->key) {
        plog_notice(plugin, "config key             : %s", placeholder);
    }
    if (config->keypass) {
        plog_notice(plugin, "config keypass         : %s", placeholder);
    }

    return 0;

error:
    free(err_param);
    free(client_id.v.val_str);
    free(write_req_topic.v.val_str);
    free(write_resp_topic.v.val_str);
    free(host.v.val_str);
    free(username.v.val_str);
    free(password.v.val_str);
    free(ca.v.val_str);
    free(cert.v.val_str);
    free(key.v.val_str);
    free(keypass.v.val_str);
    free(upload_drv_state_topic.v.val_str);
    free(driver_action_req_topic.v.val_str);
    free(driver_action_resp_topic.v.val_str);
    return -1;
}

void mqtt_config_fini(mqtt_config_t *config)
{
    free(config->client_id);
    free(config->write_req_topic);
    free(config->write_resp_topic);
    free(config->driver_action_req_topic);
    free(config->driver_action_resp_topic);
    free(config->host);
    free(config->username);
    free(config->password);
    free(config->ca);
    free(config->cert);
    free(config->key);
    free(config->keypass);
    free(config->heartbeat_topic);

    memset(config, 0, sizeof(*config));
}
