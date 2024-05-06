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

#include "utils/asprintf.h"
#include "utils/time.h"

#include "mqtt_config.h"
#include "mqtt_handle.h"
#include "mqtt_plugin.h"
#include "mqtt_plugin_intf.h"

#define AUTH_SAS 0
#define AUTH_CERT 1

static int parse_ssl_params(neu_plugin_t *plugin, const char *setting,
                            neu_json_elem_t *ca, neu_json_elem_t *cert,
                            neu_json_elem_t *key)
{
    // ca, required
    int ret = neu_parse_param(setting, NULL, 1, ca);
    if (0 != ret) {
        plog_notice(plugin, "setting no ca");
        return -1;
    }

    if (0 != decode_b64_param(plugin, ca)) {
        return -1;
    }

    // cert, required
    ret = neu_parse_param(setting, NULL, 1, cert);
    if (0 != ret) {
        plog_notice(plugin, "setting no cert");
        return -1;
    }

    if (0 != decode_b64_param(plugin, cert)) {
        return -1;
    }

    // key, required
    ret = parse_b64_param(plugin, setting, key);
    if (0 != ret) {
        return -1;
    }

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

    if (AUTH_CERT == auth.v.val_int) {
        ret = parse_ssl_params(plugin, setting, &ca, &cert, &key);
        if (0 != ret) {
            plog_error(plugin, "setting certificates fail");
            goto error;
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

    if (0 > neu_asprintf(&username, "%s/%s/?api-version=2021-04-12",
                         host.v.val_str, client_id.v.val_str)) {
        plog_error(plugin, "gen username fail");
        goto error;
    }

    config->client_id           = client_id.v.val_str;
    config->qos                 = qos.v.val_int;
    config->format              = format.v.val_int;
    config->write_req_topic     = write_req_topic;
    config->write_resp_topic    = write_resp_topic;
    config->cache               = false;
    config->cache_mem_size      = 0;
    config->cache_disk_size     = 0;
    config->cache_sync_interval = NEU_MQTT_CACHE_SYNC_INTERVAL_DEFAULT;
    config->host                = host.v.val_str;
    config->port                = 8883;
    config->username            = username;
    config->password            = password.v.val_str;
    config->ssl                 = true;
    config->ca                  = ca.v.val_str;
    config->cert                = cert.v.val_str;
    config->key                 = key.v.val_str;

    plog_notice(plugin, "config client-id       : %s", config->client_id);
    plog_notice(plugin, "config qos             : %d", config->qos);
    plog_notice(plugin, "config format          : %s",
                mqtt_upload_format_str(config->format));
    plog_notice(plugin, "config write-req-topic : %s", config->write_req_topic);
    plog_notice(plugin, "config write-resp-topic: %s",
                config->write_resp_topic);
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

    return 0;

error:
    free(err_param);
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

    if (0 !=
        neu_mqtt_client_subscribe(plugin->client, config->qos,
                                  config->write_req_topic, plugin,
                                  handle_write_req)) {
        plog_error(plugin, "subscribe [%s] fail",
                   plugin->config.write_req_topic);
        return NEU_ERR_MQTT_SUBSCRIBE_FAILURE;
    }

    return 0;
}

static int unsubscribe(neu_plugin_t *plugin, const mqtt_config_t *config)
{
    neu_mqtt_client_unsubscribe(plugin->client, config->write_req_topic);
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

    return plugin;
}

static int azure_handle_trans_data(neu_plugin_t *            plugin,
                                   neu_reqresp_trans_data_t *trans_data)
{
    int rv = 0;

    if (NULL == plugin->client) {
        return NEU_ERR_MQTT_IS_NULL;
    }

    if (0 == plugin->config.cache &&
        !neu_mqtt_client_is_connected(plugin->client)) {
        // cache disable and we are disconnected
        return NEU_ERR_MQTT_FAILURE;
    }

    char *json_str =
        generate_upload_json(plugin, trans_data, plugin->config.format);
    if (NULL == json_str) {
        plog_error(plugin, "generate upload json fail");
        return NEU_ERR_EINTERNAL;
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
        error = handle_write_response(plugin, head->ctx, data);
        break;
    case NEU_RESP_READ_GROUP:
        // error = handle_read_response(plugin, head->ctx, data);
        break;
    case NEU_REQRESP_TRANS_DATA: {
        NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_TRANS_DATA_5S, 1, NULL);
        NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_TRANS_DATA_30S, 1, NULL);
        NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_TRANS_DATA_60S, 1, NULL);
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
    .close   = mqtt_plugin_close,
    .init    = mqtt_plugin_init,
    .uninit  = mqtt_plugin_uninit,
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
