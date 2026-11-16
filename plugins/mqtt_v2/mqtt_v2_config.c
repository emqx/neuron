/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2026 EMQ Technologies Co., Ltd All rights reserved.
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
#include <string.h>

#include <jansson.h>

#include "utils/base64.h"

#include "mqtt_v2_config.h"
#include "mqtt_v2_params.h"
#include "mqtt_v2_plugin.h"

#define MB 1000000

static int get_string(neu_plugin_t *plugin, json_t *params, const char *name,
                      bool required, char **out)
{
    json_t *value = json_object_get(params, name);

    if (NULL == value || json_is_null(value)) {
        if (required) {
            plog_error(plugin, "setting no %s", name);
            return -1;
        }
        return 0;
    }

    if (!json_is_string(value)) {
        plog_error(plugin, "setting invalid %s, not a string", name);
        return -1;
    }

    *out = strdup(json_string_value(value));
    return NULL == *out ? -1 : 0;
}

// `out` keeps its default when an optional `name` is absent
static int get_int(neu_plugin_t *plugin, json_t *params, const char *name,
                   bool required, int64_t min, int64_t max, int64_t *out)
{
    json_t *value = json_object_get(params, name);

    if (NULL == value || json_is_null(value)) {
        if (required) {
            plog_error(plugin, "setting no %s", name);
            return -1;
        }
        return 0;
    }

    if (!json_is_integer(value) || json_integer_value(value) < min ||
        max < json_integer_value(value)) {
        plog_error(plugin, "setting invalid %s", name);
        return -1;
    }

    *out = json_integer_value(value);
    return 0;
}

static int get_bool(neu_plugin_t *plugin, json_t *params, const char *name,
                    bool *out)
{
    json_t *value = json_object_get(params, name);

    if (NULL == value || json_is_null(value)) {
        return 0;
    }

    if (!json_is_boolean(value)) {
        plog_error(plugin, "setting invalid %s, not a bool", name);
        return -1;
    }

    *out = json_is_true(value);
    return 0;
}

// an empty optional certificate counts as absent
static int get_b64(neu_plugin_t *plugin, json_t *params, const char *name,
                   bool required, char **out)
{
    char *text    = NULL;
    char *decoded = NULL;
    int   len     = 0;

    if (0 != get_string(plugin, params, name, required, &text)) {
        return -1;
    }

    if (NULL == text || ('\0' == text[0] && !required)) {
        free(text);
        return 0;
    }

    decoded = (char *) neu_decode64(&len, text);
    free(text);
    if (NULL == decoded || len <= 0) {
        plog_error(plugin, "setting %s invalid base64", name);
        free(decoded);
        return -1;
    }

    decoded[len] = '\0';
    *out         = decoded;
    return 0;
}

static int get_template(neu_plugin_t *plugin, json_t *params, const char *name,
                        const char *def, mqtt_v2_tmpl_kind_e kind,
                        mqtt_v2_tmpl_t **out)
{
    json_t *value    = json_object_get(params, name);
    char *  text     = NULL;
    char    err[256] = { 0 };

    if (NULL == value || json_is_null(value) ||
        (json_is_string(value) &&
         strspn(json_string_value(value), " \t\r\n") ==
             json_string_length(value))) {
        text = strdup(def);
    } else if (json_is_string(value)) {
        text = strdup(json_string_value(value));
    } else {
        text = json_dumps(value, JSON_ENCODE_ANY);
    }

    if (NULL == text) {
        return -1;
    }

    if (MQTT_V2_TEMPLATE_MAX_LEN < strlen(text)) {
        plog_error(plugin, "setting %s longer than %d", name,
                   MQTT_V2_TEMPLATE_MAX_LEN);
        free(text);
        return -1;
    }

    if (0 != mqtt_v2_tmpl_compile(text, kind, out, err, sizeof(err))) {
        plog_error(plugin, "setting invalid %s: %s", name, err);
        free(text);
        return -1;
    }

    plog_notice(plugin, "config %-20s: %s", name, text);
    free(text);
    return 0;
}

static int get_topic_prefix(neu_plugin_t *plugin, json_t *params, char **out)
{
    static const char *vars[] = { "app", NULL };
    char *             prefix = NULL;

    if (0 != get_string(plugin, params, "topic-prefix", false, &prefix)) {
        return -1;
    }

    if (NULL == prefix || '\0' == prefix[0]) {
        free(prefix);
        prefix = strdup(MQTT_V2_DEFAULT_TOPIC_PREFIX);
        if (NULL == prefix) {
            return -1;
        }
    }

    size_t len = strlen(prefix);
    while (0 < len && '/' == prefix[len - 1]) {
        prefix[--len] = '\0';
    }

    if (!mqtt_v2_topic_tmpl_valid(prefix, vars)) {
        plog_error(plugin, "setting invalid topic-prefix `%s`", prefix);
        free(prefix);
        return -1;
    }

    *out = prefix;
    return 0;
}

static int parse_cache(neu_plugin_t *plugin, json_t *params,
                       mqtt_v2_config_t *config)
{
    int64_t mem  = 0;
    int64_t disk = 0;
    int64_t sync = NEU_MQTT_CACHE_SYNC_INTERVAL_DEFAULT;

    if (0 != get_bool(plugin, params, "offline-cache", &config->cache)) {
        return -1;
    }

    if (config->cache) {
        if (0 !=
                get_int(plugin, params, "cache-mem-size", true, 1, 1024,
                        &mem) ||
            0 !=
                get_int(plugin, params, "cache-disk-size", true, 1, 10240,
                        &disk) ||
            0 !=
                get_int(plugin, params, "cache-sync-interval", false,
                        NEU_MQTT_CACHE_SYNC_INTERVAL_MIN,
                        NEU_MQTT_CACHE_SYNC_INTERVAL_MAX, &sync)) {
            return -1;
        }

        if (mem > disk) {
            plog_error(plugin,
                       "setting cache memory size %" PRIi64
                       " larger than cache disk size %" PRIi64,
                       mem, disk);
            return -1;
        }
    }

    config->cache_mem_size      = mem * MB;
    config->cache_disk_size     = disk * MB;
    config->cache_sync_interval = sync;
    return 0;
}

static int parse_ssl(neu_plugin_t *plugin, json_t *params,
                     mqtt_v2_config_t *config)
{
    if (0 != get_bool(plugin, params, "ssl", &config->ssl)) {
        return -1;
    }

    if (!config->ssl) {
        return 0;
    }

    if (0 != get_b64(plugin, params, "ca", true, &config->ca) ||
        0 != get_b64(plugin, params, "cert", false, &config->cert)) {
        return -1;
    }

    // the key is required when the client cert is set
    if (NULL != config->cert &&
        0 != get_b64(plugin, params, "key", true, &config->key)) {
        return -1;
    }

    return 0;
}

static void log_config(neu_plugin_t *plugin, const mqtt_v2_config_t *config)
{
    const char *placeholder = "********";

    plog_notice(plugin, "config MQTT version       : %d", config->version);
    plog_notice(plugin, "config client-id          : %s", config->client_id);
    plog_notice(plugin, "config qos                : %d", config->qos);
    plog_notice(plugin, "config topic-prefix       : %s", config->topic_prefix);
    plog_notice(plugin, "config upload-err         : %d", config->upload_err);
    plog_notice(plugin, "config offline-cache      : %d", config->cache);
    plog_notice(plugin, "config cache-mem-size     : %zu",
                config->cache_mem_size);
    plog_notice(plugin, "config cache-disk-size    : %zu",
                config->cache_disk_size);
    plog_notice(plugin, "config cache-sync-interval: %zu",
                config->cache_sync_interval);
    plog_notice(plugin, "config host               : %s", config->host);
    plog_notice(plugin, "config port               : %" PRIu16, config->port);
    if (config->username) {
        plog_notice(plugin, "config username           : %s", config->username);
    }
    if (config->password) {
        plog_notice(plugin, "config password           : %s",
                    '\0' == config->password[0] ? "" : placeholder);
    }
    plog_notice(plugin, "config ssl                : %d", config->ssl);
    if (config->ca) {
        plog_notice(plugin, "config ca                 : %s", placeholder);
    }
    if (config->cert) {
        plog_notice(plugin, "config cert               : %s", placeholder);
    }
    if (config->key) {
        plog_notice(plugin, "config key                : %s", placeholder);
    }
}

int mqtt_v2_config_parse(neu_plugin_t *plugin, const char *setting,
                         mqtt_v2_config_t *config)
{
    int64_t version = NEU_MQTT_VERSION_V311;
    int64_t qos     = NEU_MQTT_QOS0;
    int64_t port    = 0;
    json_t *root    = NULL;
    json_t *params  = NULL;

    memset(config, 0, sizeof(*config));
    config->upload_err = true;

    root   = json_loads(setting, 0, NULL);
    params = json_object_get(root, "params");
    if (!json_is_object(params)) {
        plog_error(plugin, "setting no params");
        goto error;
    }

    if (0 !=
            get_int(plugin, params, "version", false, NEU_MQTT_VERSION_V311,
                    NEU_MQTT_VERSION_V5, &version) ||
        0 !=
            get_string(plugin, params, "client-id", true, &config->client_id) ||
        0 !=
            get_int(plugin, params, "qos", false, NEU_MQTT_QOS0, NEU_MQTT_QOS2,
                    &qos) ||
        0 != get_topic_prefix(plugin, params, &config->topic_prefix) ||
        0 !=
            get_template(plugin, params, "data-template",
                         MQTT_V2_DEFAULT_DATA_TEMPLATE, MQTT_V2_TMPL_DATA,
                         &config->data_tmpl) ||
        0 !=
            get_template(plugin, params, "tag-template",
                         MQTT_V2_DEFAULT_TAG_TEMPLATE, MQTT_V2_TMPL_TAG,
                         &config->tag_tmpl) ||
        0 != get_bool(plugin, params, "upload-err", &config->upload_err) ||
        0 != parse_cache(plugin, params, config) ||
        0 != get_string(plugin, params, "host", true, &config->host) ||
        0 != get_int(plugin, params, "port", true, 1, 65535, &port) ||
        0 != get_string(plugin, params, "username", false, &config->username) ||
        0 != get_string(plugin, params, "password", false, &config->password) ||
        0 != parse_ssl(plugin, params, config)) {
        goto error;
    }

    if ('\0' == config->client_id[0]) {
        plog_error(plugin, "setting empty client-id");
        goto error;
    }

    if ('\0' == config->host[0]) {
        plog_error(plugin, "setting empty host");
        goto error;
    }

    config->version = version;
    config->qos     = qos;
    config->port    = port;

    log_config(plugin, config);
    json_decref(root);
    return 0;

error:
    json_decref(root);
    mqtt_v2_config_fini(config);
    return -1;
}

void mqtt_v2_config_fini(mqtt_v2_config_t *config)
{
    free(config->client_id);
    free(config->topic_prefix);
    mqtt_v2_tmpl_free(config->data_tmpl);
    mqtt_v2_tmpl_free(config->tag_tmpl);
    free(config->host);
    free(config->username);
    free(config->password);
    free(config->ca);
    free(config->cert);
    free(config->key);
    memset(config, 0, sizeof(*config));
}
