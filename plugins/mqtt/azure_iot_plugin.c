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
#include <string.h>

#include "utils/asprintf.h"
#include "utils/time.h"

#include "mqtt_config.h"
#include "mqtt_handle.h"
#include "mqtt_plugin.h"
#include "mqtt_plugin_intf.h"

#define AUTH_SAS 0
#define AUTH_CERT 1

#define MB 1000000

#define AZURE_PENDING_MAGIC 0x415a4454u
#define AZURE_PENDING_DESIRED 1
#define AZURE_PENDING_METHOD 2

#define AZURE_DESIRED_TOPIC "$iothub/twin/PATCH/properties/desired/#"
#define AZURE_METHOD_TOPIC "$iothub/methods/POST/#"
#define AZURE_TWIN_RES_TOPIC "$iothub/twin/res/#"

typedef struct azure_tag_route {
    char           tag[NEU_TAG_NAME_LEN];
    char           driver[NEU_NODE_NAME_LEN];
    char           group[NEU_GROUP_NAME_LEN];
    UT_hash_handle hh;
} azure_tag_route_t;

typedef struct azure_pending {
    uint32_t magic;
    int      kind;
    char *   rid;
    char *   name;
    char *   value_json;
    int64_t  version;
} azure_pending_t;

static uint32_t azure_request_id = 1;

static bool azure_model_mode(const mqtt_config_t *config)
{
    return NULL != config->username && NULL != strstr(config->username, "&model-id=");
}

static char *azure_url_encode(const char *src)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t            len   = strlen(src);
    char *            out   = malloc(len * 3 + 1);
    char *            p     = out;

    if (NULL == out) {
        return NULL;
    }
    for (; *src != '\0'; ++src) {
        unsigned char c = (unsigned char) *src;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
            c == '~') {
            *p++ = (char) c;
        } else {
            *p++ = '%';
            *p++ = hex[c >> 4];
            *p++ = hex[c & 0x0f];
        }
    }
    *p = '\0';
    return out;
}

static void azure_routes_free(neu_plugin_t *plugin)
{
    azure_tag_route_t *head  = plugin->app_data;
    azure_tag_route_t *route = NULL;
    azure_tag_route_t *tmp   = NULL;

    HASH_ITER(hh, head, route, tmp)
    {
        HASH_DEL(head, route);
        free(route);
    }
    plugin->app_data = head;
}

static void azure_route_put(neu_plugin_t *plugin, const char *tag,
                            const char *driver, const char *group)
{
    azure_tag_route_t *head  = plugin->app_data;
    azure_tag_route_t *route = NULL;

    if (NULL == tag || '\0' == tag[0] || strlen(tag) >= NEU_TAG_NAME_LEN ||
        NULL == driver || strlen(driver) >= NEU_NODE_NAME_LEN || NULL == group ||
        strlen(group) >= NEU_GROUP_NAME_LEN) {
        return;
    }

    HASH_FIND_STR(head, tag, route);
    if (NULL == route) {
        route = calloc(1, sizeof(*route));
        if (NULL == route) {
            return;
        }
        strncpy(route->tag, tag, sizeof(route->tag) - 1);
        HASH_ADD_STR(head, tag, route);
        plugin->app_data = head;
    }
    strncpy(route->driver, driver, sizeof(route->driver) - 1);
    strncpy(route->group, group, sizeof(route->group) - 1);
}

static azure_tag_route_t *azure_route_get(neu_plugin_t *plugin, const char *tag)
{
    azure_tag_route_t *head  = plugin->app_data;
    azure_tag_route_t *route = NULL;
    HASH_FIND_STR(head, tag, route);
    return route;
}

static void azure_pending_free(azure_pending_t *pending)
{
    if (NULL == pending) {
        return;
    }
    free(pending->rid);
    free(pending->name);
    free(pending->value_json);
    free(pending);
}

static bool azure_is_pending(const void *ctx)
{
    const azure_pending_t *pending = ctx;
    return NULL != pending && AZURE_PENDING_MAGIC == pending->magic;
}

static int parse_cache_params(neu_plugin_t *plugin, const char *setting,
                              mqtt_config_t *config)
{
    int             ret                 = 0;
    char *          err_param           = NULL;
    neu_json_elem_t offline_cache       = { .name = "offline-cache",
                                      .t    = NEU_JSON_BOOL };
    neu_json_elem_t cache_mem_size      = { .name = "cache-mem-size",
                                       .t    = NEU_JSON_INT };
    neu_json_elem_t cache_disk_size     = { .name = "cache-disk-size",
                                        .t    = NEU_JSON_INT };
    neu_json_elem_t cache_sync_interval = { .name = "cache-sync-interval",
                                            .t    = NEU_JSON_INT };

    // offline-cache flag, optional (backward compatible: missing -> disabled)
    ret = neu_parse_param(setting, NULL, 1, &offline_cache);
    if (0 != ret || !offline_cache.v.val_bool) {
        config->cache               = false;
        config->cache_mem_size      = 0;
        config->cache_disk_size     = 0;
        config->cache_sync_interval = NEU_MQTT_CACHE_SYNC_INTERVAL_DEFAULT;
        return 0;
    }

    // cache enabled, mem and disk size are required
    ret = neu_parse_param(setting, &err_param, 2, &cache_mem_size,
                          &cache_disk_size);
    if (0 != ret) {
        plog_error(plugin, "parsing cache size fail, key: `%s`", err_param);
        free(err_param);
        return -1;
    }

    if (cache_mem_size.v.val_int < 1 || cache_mem_size.v.val_int > 1024) {
        plog_error(plugin, "setting invalid cache memory size: %" PRIi64,
                   cache_mem_size.v.val_int);
        return -1;
    }

    if (cache_disk_size.v.val_int < 1 || cache_disk_size.v.val_int > 10240) {
        plog_error(plugin, "setting invalid cache disk size: %" PRIi64,
                   cache_disk_size.v.val_int);
        return -1;
    }

    if (cache_mem_size.v.val_int > cache_disk_size.v.val_int) {
        plog_error(plugin,
                   "setting cache memory size %" PRIi64
                   " larger than cache disk size %" PRIi64,
                   cache_mem_size.v.val_int, cache_disk_size.v.val_int);
        return -1;
    }

    // cache-sync-interval, optional
    ret = neu_parse_param(setting, NULL, 1, &cache_sync_interval);
    if (0 == ret) {
        if (cache_sync_interval.v.val_int < NEU_MQTT_CACHE_SYNC_INTERVAL_MIN ||
            NEU_MQTT_CACHE_SYNC_INTERVAL_MAX < cache_sync_interval.v.val_int) {
            plog_error(plugin, "setting invalid cache sync interval: %" PRIi64,
                       cache_sync_interval.v.val_int);
            return -1;
        }
    } else {
        cache_sync_interval.v.val_int = NEU_MQTT_CACHE_SYNC_INTERVAL_DEFAULT;
    }

    config->cache               = true;
    config->cache_mem_size      = cache_mem_size.v.val_int * MB;
    config->cache_disk_size     = cache_disk_size.v.val_int * MB;
    config->cache_sync_interval = cache_sync_interval.v.val_int;
    return 0;
}

static int azure_parse_config(neu_plugin_t *plugin, const char *setting,
                              mqtt_config_t *config)
{
    int         ret              = 0;
    char *      err_param        = NULL;
    const char *placeholder      = "********";
    char *      write_req_topic  = NULL;
    char *      write_resp_topic = NULL;
    char *      username         = NULL;
    char *      model_id         = NULL;

    neu_json_elem_t client_id = { .name = "device-id", .t = NEU_JSON_STR };
    neu_json_elem_t qos       = { .name = "qos", .t = NEU_JSON_INT };
    neu_json_elem_t format    = { .name = "format", .t = NEU_JSON_INT };
    neu_json_elem_t host      = { .name = "host", .t = NEU_JSON_STR };
    neu_json_elem_t auth      = { .name = "auth", .t = NEU_JSON_INT };
    neu_json_elem_t password  = { .name = "sas", .t = NEU_JSON_STR };
    neu_json_elem_t ca        = { .name = "ca", .t = NEU_JSON_STR };
    neu_json_elem_t cert      = { .name = "cert", .t = NEU_JSON_STR };
    neu_json_elem_t key       = { .name = "key", .t = NEU_JSON_STR };

    if (NULL == setting || NULL == config) {
        plog_error(plugin, "invalid argument, null pointer");
        return -1;
    }

    ret = neu_parse_param(setting, &err_param, 5, &client_id, &qos, &format,
                          &host, &auth);
    if (0 != ret) {
        plog_error(plugin, "parsing setting fail, key: `%s`", err_param);
        goto error;
    }

    // client-id, required
    if (0 == strlen(client_id.v.val_str)) {
        plog_error(plugin, "setting empty client-id");
        goto error;
    }

    // qos, required
    if (qos.v.val_int != NEU_MQTT_QOS0 && NEU_MQTT_QOS1 != qos.v.val_int) {
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

    // host, required
    if (0 == strlen(host.v.val_str)) {
        plog_error(plugin, "setting invalid host: `%s`", host.v.val_str);
        goto error;
    }

    if (AUTH_SAS != auth.v.val_int && AUTH_CERT != auth.v.val_int) {
        plog_error(plugin, "setting invalid auth %" PRIu64, auth.v.val_int);
        goto error;
    }

    // password
    if (AUTH_SAS == auth.v.val_int) {
        ret = neu_parse_param(setting, NULL, 1, &password);
        if (0 != ret) {
            plog_error(plugin, "setting no password");
            goto error;
        }
    } else {
        password.v.val_str = strdup("");
    }

    // ca, required for TLS (SSL is always enabled)
    ret = neu_parse_param(setting, NULL, 1, &ca);
    if (0 != ret) {
        plog_error(plugin, "setting no ca");
        goto error;
    }
    if (0 != decode_b64_param(plugin, &ca)) {
        goto error;
    }

    if (AUTH_CERT == auth.v.val_int) {
        ret = neu_parse_param(setting, NULL, 1, &cert);
        if (0 != ret) {
            plog_error(plugin, "setting no cert");
            goto error;
        }
        if (0 != decode_b64_param(plugin, &cert)) {
            goto error;
        }
        ret = parse_b64_param(plugin, setting, &key);
        if (0 != ret) {
            plog_error(plugin, "setting no key");
            goto error;
        }
    }

    // offline cache
    if (0 != parse_cache_params(plugin, setting, config)) {
        goto error;
    }

    {
        neu_json_elem_t model_elem = { .name = "model-id", .t = NEU_JSON_STR };
        if (0 == neu_parse_param(setting, NULL, 1, &model_elem)) {
            if (NULL != model_elem.v.val_str && '\0' != model_elem.v.val_str[0]) {
                if (strlen(model_elem.v.val_str) > 255) {
                    plog_error(plugin, "setting model-id too long");
                    free(model_elem.v.val_str);
                    goto error;
                }
                model_id = model_elem.v.val_str;
            } else {
                free(model_elem.v.val_str);
            }
        }
    }

    // write request topic
    if (0 > neu_asprintf(&write_req_topic, "devices/%s/messages/devicebound/#",
                         client_id.v.val_str)) {
        plog_error(plugin, "setting write request topic error");
        goto error;
    }

    // write response topic
    if (0 > neu_asprintf(&write_resp_topic, "devices/%s/messages/events/",
                         client_id.v.val_str)) {
        plog_error(plugin, "setting write response topic error");
        goto error;
    }

    if (NULL == model_id) {
        if (0 > neu_asprintf(&username, "%s/%s/?api-version=2021-04-12",
                             host.v.val_str, client_id.v.val_str)) {
            plog_error(plugin, "gen username fail");
            goto error;
        }
    } else {
        char *encoded = azure_url_encode(model_id);
        if (NULL == encoded ||
            0 > neu_asprintf(&username,
                             "%s/%s/?api-version=2021-04-12&model-id=%s",
                             host.v.val_str, client_id.v.val_str, encoded)) {
            plog_error(plugin, "gen username fail");
            free(encoded);
            goto error;
        }
        free(encoded);
        free(model_id);
        model_id = NULL;
    }

    config->version          = NEU_MQTT_VERSION_V311;
    config->client_id        = client_id.v.val_str;
    config->qos              = qos.v.val_int;
    config->format           = format.v.val_int;
    config->write_req_topic  = write_req_topic;
    config->write_resp_topic = write_resp_topic;
    config->host             = host.v.val_str;
    config->port             = 8883;
    config->username         = username;
    config->password         = password.v.val_str;
    config->ssl              = true;
    config->ca               = ca.v.val_str;
    config->cert             = cert.v.val_str;
    config->key              = key.v.val_str;
    config->upload_drv_state = false;
    config->enable_topic     = true;

    plog_notice(plugin, "config MQTT version    : %d", config->version);
    plog_notice(plugin, "config client-id       : %s", config->client_id);
    plog_notice(plugin, "config qos             : %d", config->qos);
    plog_notice(plugin, "config format          : %s",
                mqtt_upload_format_str(config->format));
    plog_notice(plugin, "config write-req-topic : %s", config->write_req_topic);
    plog_notice(plugin, "config write-resp-topic: %s",
                config->write_resp_topic);
    plog_notice(plugin, "config upload-drv-state: %d",
                config->upload_drv_state);
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
    plog_notice(plugin, "config dtdl            : %d",
                azure_model_mode(config));
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

    return 0;

error:
    free(err_param);
    free(model_id);
    free(client_id.v.val_str);
    free(write_req_topic);
    free(write_resp_topic);
    free(host.v.val_str);
    free(username);
    free(password.v.val_str);
    free(ca.v.val_str);
    free(cert.v.val_str);
    free(key.v.val_str);
    return -1;
}

static int azure_publish_copy(neu_plugin_t *plugin, const char *topic,
                              const char *payload)
{
    char *topic_copy   = strdup(topic);
    char *payload_copy = strdup(payload);
    int   rv           = 0;

    if (NULL == topic_copy || NULL == payload_copy) {
        free(topic_copy);
        free(payload_copy);
        return -1;
    }
    rv = publish(plugin, NEU_MQTT_QOS0, topic_copy, payload_copy,
                 strlen(payload_copy));
    free(topic_copy);
    return rv;
}

static void azure_publish_reported(neu_plugin_t *plugin, const char *name,
                                   const char *value_json, int code,
                                   int64_t version)
{
    json_t *      value    = NULL;
    json_t *      item     = NULL;
    json_t *      root     = NULL;
    char *        payload  = NULL;
    char          topic[128];
    json_error_t  error    = { 0 };
    uint32_t      rid      = azure_request_id++;

    value = json_loads(value_json, JSON_DECODE_ANY, &error);
    if (NULL == value) {
        plog_error(plugin, "reported value is not json");
        return;
    }
    item = json_object();
    root = json_object();
    if (NULL == item || NULL == root) {
        json_decref(value);
        json_decref(item);
        json_decref(root);
        return;
    }
    json_object_set_new(item, "value", value);
    json_object_set_new(item, "ac", json_integer(code));
    json_object_set_new(item, "av", json_integer(version));
    json_object_set_new(root, name, item);
    payload = json_dumps(root, JSON_REAL_PRECISION(16));
    json_decref(root);
    if (NULL == payload) {
        return;
    }
    snprintf(topic, sizeof(topic),
             "$iothub/twin/PATCH/properties/reported/?$rid=%" PRIu32, rid);
    if (0 != azure_publish_copy(plugin, topic, payload)) {
        plog_error(plugin, "publish twin reported fail");
    }
    free(payload);
}

static void azure_publish_method_res(neu_plugin_t *plugin, const char *rid,
                                     int status)
{
    char topic[160];

    snprintf(topic, sizeof(topic), "$iothub/methods/res/%d/?$rid=%s", status,
             rid);
    if (0 != azure_publish_copy(plugin, topic, "{}")) {
        plog_error(plugin, "publish direct method response fail");
    }
}

static int azure_finish_write(neu_plugin_t *plugin, azure_pending_t *pending,
                              neu_resp_error_t *resp)
{
    int ok = NULL != resp && NEU_ERR_SUCCESS == resp->error;

    if (AZURE_PENDING_DESIRED == pending->kind) {
        azure_publish_reported(plugin, pending->name, pending->value_json,
                               ok ? 200 : 500, pending->version);
    } else {
        azure_publish_method_res(plugin, pending->rid, ok ? 200 : 500);
    }
    azure_pending_free(pending);
    return NEU_ERR_SUCCESS;
}

static int json_to_dvalue(json_t *val, neu_dvalue_t *out)
{
    memset(out, 0, sizeof(*out));
    if (json_is_integer(val)) {
        out->type      = NEU_TYPE_INT64;
        out->value.i64 = json_integer_value(val);
        return 0;
    }
    if (json_is_real(val)) {
        out->type      = NEU_TYPE_DOUBLE;
        out->value.d64 = json_real_value(val);
        return 0;
    }
    if (json_is_boolean(val)) {
        out->type          = NEU_TYPE_BOOL;
        out->value.boolean = json_is_true(val);
        return 0;
    }
    if (json_is_string(val)) {
        const char *text = json_string_value(val);
        if (NULL == text || strlen(text) >= NEU_VALUE_SIZE) {
            return -1;
        }
        out->type = NEU_TYPE_STRING;
        strncpy(out->value.str, text, sizeof(out->value.str) - 1);
        return 0;
    }
    return -1;
}

static int azure_write_value(neu_plugin_t *plugin, const char *name,
                             json_t *value, int kind, const char *rid,
                             int64_t version)
{
    azure_tag_route_t *route   = azure_route_get(plugin, name);
    azure_pending_t *  pending = NULL;
    neu_reqresp_head_t header  = { 0 };
    neu_req_write_tag_t cmd    = { 0 };
    char *             dumped  = NULL;

    dumped = json_dumps(value, JSON_ENCODE_ANY | JSON_REAL_PRECISION(16));
    if (NULL == dumped) {
        return -1;
    }
    if (NULL == route) {
        plog_notice(plugin, "no south tag for `%s`", name);
        if (AZURE_PENDING_DESIRED == kind) {
            azure_publish_reported(plugin, name, dumped, 404, version);
        } else if (NULL != rid) {
            azure_publish_method_res(plugin, rid, 404);
        }
        free(dumped);
        return 0;
    }
    if (0 != json_to_dvalue(value, &cmd.value)) {
        plog_error(plugin, "unsupported value for `%s`", name);
        if (AZURE_PENDING_DESIRED == kind) {
            azure_publish_reported(plugin, name, dumped, 400, version);
        } else if (NULL != rid) {
            azure_publish_method_res(plugin, rid, 400);
        }
        free(dumped);
        return 0;
    }

    pending = calloc(1, sizeof(*pending));
    if (NULL == pending) {
        free(dumped);
        return -1;
    }
    pending->magic      = AZURE_PENDING_MAGIC;
    pending->kind       = kind;
    pending->name       = strdup(name);
    pending->value_json = dumped;
    pending->version    = version;
    pending->rid        = NULL != rid ? strdup(rid) : NULL;
    if (NULL == pending->name || NULL == pending->value_json ||
        (NULL != rid && NULL == pending->rid)) {
        azure_pending_free(pending);
        return -1;
    }

    header.ctx  = pending;
    header.type = NEU_REQ_WRITE_TAG;
    cmd.driver  = strdup(route->driver);
    cmd.group   = strdup(route->group);
    cmd.tag     = strdup(name);
    if (NULL == cmd.driver || NULL == cmd.group || NULL == cmd.tag ||
        0 != neu_plugin_op(plugin, header, &cmd)) {
        plog_error(plugin, "write tag `%s` fail", name);
        free(cmd.driver);
        free(cmd.group);
        free(cmd.tag);
        if (AZURE_PENDING_DESIRED == kind) {
            azure_publish_reported(plugin, name, pending->value_json, 500,
                                   version);
        } else if (NULL != rid) {
            azure_publish_method_res(plugin, rid, 500);
        }
        azure_pending_free(pending);
        return -1;
    }
    return 0;
}

static void azure_apply_properties(neu_plugin_t *plugin, json_t *object,
                                   int64_t version)
{
    const char *key = NULL;
    json_t *    val = NULL;

    json_object_foreach(object, key, val)
    {
        if ('$' == key[0]) {
            continue;
        }
        azure_write_value(plugin, key, val, AZURE_PENDING_DESIRED, NULL,
                          version);
    }
}

static void azure_on_twin(neu_mqtt_qos_e qos, const char *topic,
                          const uint8_t *payload, uint32_t len, void *data,
                          trace_w3c_t *trace_w3c)
{
    neu_plugin_t *plugin  = data;
    json_error_t  error   = { 0 };
    json_t *      root    = NULL;
    json_t *      desired = NULL;
    json_t *      version = NULL;
    int64_t       ver     = 0;

    (void) qos;
    (void) trace_w3c;
    root = json_loadb((const char *) payload, len, 0, &error);
    if (!json_is_object(root)) {
        json_decref(root);
        plog_error(plugin, "twin payload is not an object");
        return;
    }
    if (NULL != strstr(topic, "$iothub/twin/res/")) {
        if (NULL == strstr(topic, "/res/200/")) {
            json_decref(root);
            return;
        }
        desired = json_object_get(root, "desired");
    } else {
        desired = root;
    }
    if (!json_is_object(desired)) {
        json_decref(root);
        return;
    }
    version = json_object_get(desired, "$version");
    if (json_is_integer(version)) {
        ver = json_integer_value(version);
    }
    azure_apply_properties(plugin, desired, ver);
    json_decref(root);
}

static void azure_on_method(neu_mqtt_qos_e qos, const char *topic,
                            const uint8_t *payload, uint32_t len, void *data,
                            trace_w3c_t *trace_w3c)
{
    neu_plugin_t *plugin = data;
    const char *  post   = NULL;
    const char *  slash  = NULL;
    const char *  rid    = NULL;
    char          name[NEU_TAG_NAME_LEN];
    json_error_t  error = { 0 };
    json_t *      body  = NULL;
    size_t        nlen  = 0;

    (void) qos;
    (void) trace_w3c;
    post = strstr(topic, "/POST/");
    rid  = strstr(topic, "$rid=");
    if (NULL == post || NULL == rid) {
        plog_error(plugin, "direct method topic `%s` missing name or rid",
                   topic);
        return;
    }
    post += strlen("/POST/");
    slash = strchr(post, '/');
    nlen  = NULL == slash ? strlen(post) : (size_t) (slash - post);
    if (0 == nlen || nlen >= sizeof(name)) {
        azure_publish_method_res(plugin, rid + 5, 404);
        return;
    }
    memcpy(name, post, nlen);
    name[nlen] = '\0';
    rid += 5;

    body = json_loadb((const char *) payload, len, JSON_DECODE_ANY, &error);
    if (NULL == body) {
        azure_publish_method_res(plugin, rid, 400);
        return;
    }
    azure_write_value(plugin, name, body, AZURE_PENDING_METHOD, rid, 0);
    json_decref(body);
}

static void azure_remember_payload(neu_plugin_t *plugin, const char *json)
{
    char *copy = strdup(json);
    if (NULL == copy) {
        return;
    }
    free(plugin->model_payload);
    plugin->model_payload = copy;
}

static void azure_publish_model_payload(neu_plugin_t *plugin)
{
    char *copy = NULL;

    if (!azure_model_mode(&plugin->config) || NULL == plugin->model_payload ||
        NULL == plugin->upload_topic || NULL == plugin->client ||
        !neu_mqtt_client_is_connected(plugin->client)) {
        return;
    }
    copy = strdup(plugin->model_payload);
    if (NULL == copy) {
        return;
    }
    publish(plugin, plugin->config.qos, plugin->upload_topic, copy, strlen(copy));
}

static int azure_model_timer(void *data)
{
    azure_publish_model_payload(data);
    return 0;
}

static void azure_start_model_timer(neu_plugin_t *plugin)
{
    neu_event_timer_param_t param = {
        .second      = 10,
        .millisecond = 0,
        .cb          = azure_model_timer,
        .usr_data    = plugin,
    };

    if (NULL != plugin->model_timer || !azure_model_mode(&plugin->config)) {
        return;
    }
    if (NULL == plugin->events) {
        plugin->events = neu_event_new(plugin->common.name);
        if (NULL == plugin->events) {
            return;
        }
    }
    plugin->model_timer = neu_event_add_timer(plugin->events, param);
}

static void azure_on_connected(neu_plugin_t *plugin)
{
    azure_start_model_timer(plugin);
    azure_publish_model_payload(plugin);
}

static void azure_request_twin(neu_plugin_t *plugin)
{
    if (NULL == plugin->client || !neu_mqtt_client_is_connected(plugin->client)) {
        return;
    }
    if (0 != azure_publish_copy(plugin, "$iothub/twin/GET/?$rid=1", "")) {
        plog_notice(plugin, "request twin failed");
    }
}

static int subscribe(neu_plugin_t *plugin, const mqtt_config_t *config)
{
    // write request topic
    if (NULL == plugin->upload_topic ||
        0 != strstr(plugin->upload_topic, config->client_id)) {
        free(plugin->upload_topic);
        plugin->upload_topic = NULL;
        neu_asprintf(&plugin->upload_topic, "devices/%s/messages/events/",
                     config->client_id);
        if (NULL == plugin->upload_topic) {
            plog_error(plugin, "create upload topic fail");
            return NEU_ERR_EINTERNAL;
        }
    }

    if (!azure_model_mode(config)) {
        if (0 !=
            neu_mqtt_client_subscribe(plugin->client, config->qos,
                                      config->write_req_topic, plugin,
                                      handle_write_req)) {
            plog_error(plugin, "subscribe [%s] fail", config->write_req_topic);
            return NEU_ERR_MQTT_SUBSCRIBE_FAILURE;
        }
        return 0;
    }

    if (0 != neu_mqtt_client_subscribe(plugin->client, config->qos,
                                       AZURE_DESIRED_TOPIC, plugin,
                                       azure_on_twin) ||
        0 != neu_mqtt_client_subscribe(plugin->client, config->qos,
                                       AZURE_TWIN_RES_TOPIC, plugin,
                                       azure_on_twin) ||
        0 != neu_mqtt_client_subscribe(plugin->client, config->qos,
                                       AZURE_METHOD_TOPIC, plugin,
                                       azure_on_method)) {
        plog_error(plugin, "subscribe twin or method topic fail");
        return NEU_ERR_MQTT_SUBSCRIBE_FAILURE;
    }
    azure_request_twin(plugin);
    return 0;
}

static int unsubscribe(neu_plugin_t *plugin, const mqtt_config_t *config)
{
    if (azure_model_mode(config)) {
        neu_mqtt_client_unsubscribe(plugin->client, AZURE_DESIRED_TOPIC);
        neu_mqtt_client_unsubscribe(plugin->client, AZURE_TWIN_RES_TOPIC);
        neu_mqtt_client_unsubscribe(plugin->client, AZURE_METHOD_TOPIC);
    } else {
        neu_mqtt_client_unsubscribe(plugin->client, config->write_req_topic);
    }
    neu_msleep(100); // wait for message completion
    return 0;
}

static neu_plugin_t *azure_plugin_open(void)
{
    neu_plugin_t *plugin = (neu_plugin_t *) calloc(1, sizeof(neu_plugin_t));
    neu_plugin_common_init(&plugin->common);
    plugin->parse_config = azure_parse_config;
    plugin->subscribe    = subscribe;
    plugin->unsubscribe  = unsubscribe;
    plugin->on_connected = azure_on_connected;

    return plugin;
}

static void azure_stop_model_timer(neu_plugin_t *plugin)
{
    if (NULL != plugin->model_timer && NULL != plugin->events) {
        neu_event_del_timer(plugin->events, plugin->model_timer);
    }
    plugin->model_timer = NULL;
}

static int azure_plugin_uninit(neu_plugin_t *plugin)
{
    azure_stop_model_timer(plugin);
    azure_routes_free(plugin);
    free(plugin->model_payload);
    plugin->model_payload = NULL;
    return mqtt_plugin_uninit(plugin);
}

static int azure_plugin_close(neu_plugin_t *plugin)
{
    azure_stop_model_timer(plugin);
    azure_routes_free(plugin);
    free(plugin->model_payload);
    plugin->model_payload = NULL;
    return mqtt_plugin_close(plugin);
}

static int azure_handle_trans_data(neu_plugin_t *            plugin,
                                   neu_reqresp_trans_data_t *trans_data)
{
    int rv = 0;

    if (NULL == plugin->client) {
        return NEU_ERR_MQTT_IS_NULL;
    }

    // DTDL mode keeps the sample and publishes it once the session connects.
    if (!azure_model_mode(&plugin->config) && 0 == plugin->config.cache &&
        !neu_mqtt_client_is_connected(plugin->client)) {
        return NEU_ERR_MQTT_FAILURE;
    }

    char *json_str = NULL;
    if (azure_model_mode(&plugin->config)) {
        utarray_foreach(trans_data->tags, neu_resp_tag_value_meta_t *, tag_value)
        {
            azure_route_put(plugin, tag_value->tag, trans_data->driver,
                            trans_data->group);
        }
        json_str = generate_flat_upload_json(plugin, trans_data);
        if (NULL == json_str) {
            return NEU_ERR_SUCCESS;
        }
        azure_remember_payload(plugin, json_str);
        if (NULL == plugin->client ||
            !neu_mqtt_client_is_connected(plugin->client)) {
            free(json_str);
            return NEU_ERR_SUCCESS;
        }
    } else {
        json_str = generate_upload_json(plugin, trans_data, plugin->config.format,
                                        NULL, 0, NULL, 0, NULL);
        if (NULL == json_str) {
            plog_error(plugin, "generate upload json fail");
            return NEU_ERR_EINTERNAL;
        }
    }

    char *         topic = plugin->upload_topic;
    neu_mqtt_qos_e qos   = plugin->config.qos;
    rv       = publish(plugin, qos, topic, json_str, strlen(json_str));
    json_str = NULL;

    return rv;
}

static int azure_plugin_request(neu_plugin_t *plugin, neu_reqresp_head_t *head,
                                void *data)
{
    neu_err_code_e error = NEU_ERR_SUCCESS;

    // update cached messages number per seconds
    if (NULL != plugin->client &&
        (global_timestamp - plugin->cache_metric_update_ts) >= 1000) {
        NEU_PLUGIN_UPDATE_METRIC(
            plugin, NEU_METRIC_CACHED_MSGS_NUM,
            neu_mqtt_client_get_cached_msgs_num(plugin->client), NULL);
        plugin->cache_metric_update_ts = global_timestamp;
    }

    switch (head->type) {
    case NEU_RESP_ERROR:
        if (azure_is_pending(head->ctx)) {
            error = azure_finish_write(plugin, head->ctx, data);
        } else {
            error = handle_write_response(plugin, head->ctx, data, NULL, NULL,
                                          NULL);
        }
        break;
    case NEU_RESP_READ_GROUP:
        // error = handle_read_response(plugin, head->ctx, data);
        break;
    case NEU_REQRESP_TRANS_DATA: {
        if (plugin->client && neu_mqtt_client_is_open(plugin->client)) {
            NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_TRANS_DATA_5S, 1, NULL);
            NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_TRANS_DATA_30S, 1,
                                     NULL);
            NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_TRANS_DATA_60S, 1,
                                     NULL);
        }
        error = azure_handle_trans_data(plugin, data);
        break;
    }
    case NEU_REQ_SUBSCRIBE_GROUP:
    case NEU_REQ_UPDATE_SUBSCRIBE_GROUP: {
        neu_req_subscribe_t *sub_info = data;
        free(sub_info->params);
        break;
    }
    case NEU_REQ_UNSUBSCRIBE_GROUP:
    case NEU_REQ_UPDATE_GROUP:
    case NEU_REQ_DEL_GROUP:
    case NEU_REQ_UPDATE_NODE:
    case NEU_REQRESP_NODE_DELETED:
        break;
    default:
        error = NEU_ERR_MQTT_FAILURE;
        break;
    }

    return error;
}

const neu_plugin_intf_funs_t azure_plugin_intf_funs = {
    .open    = azure_plugin_open,
    .close   = azure_plugin_close,
    .init    = mqtt_plugin_init,
    .uninit  = azure_plugin_uninit,
    .start   = mqtt_plugin_start,
    .stop    = mqtt_plugin_stop,
    .setting = mqtt_plugin_config,
    .request = azure_plugin_request,
};

#define DESCRIPTION "Northbound plugin for connecting to Azure IoT Hub"
#define DESCRIPTION_ZH "基于 NanoSDK 的北向应用插件，用于对接 Azure IoT Hub"

const neu_plugin_module_t neu_plugin_module = {
    .version         = NEURON_PLUGIN_VER_1_0,
    .schema          = "azure-iot",
    .module_name     = "Azure IoT",
    .module_descr    = DESCRIPTION,
    .module_descr_zh = DESCRIPTION_ZH,
    .intf_funs       = &azure_plugin_intf_funs,
    .kind            = NEU_PLUGIN_KIND_SYSTEM,
    .type            = NEU_NA_TYPE_APP,
    .display         = true,
    .single          = false,
};
