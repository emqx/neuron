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

#include "connection/mqtt_client.h"
#include "errcodes.h"
#include "utils/asprintf.h"
#include "version.h"
#include "json/neu_json_mqtt.h"
#include "json/neu_json_rw.h"

#include "mqtt_handle.h"
#include "mqtt_plugin.h"

static int tag_values_to_json(neu_resp_tag_value_t *tags, uint16_t len,
                              neu_json_read_resp_t *json)
{
    if (0 == len) {
        return 0;
    }

    json->n_tag = len;
    json->tags  = (neu_json_read_resp_tag_t *) calloc(
        json->n_tag, sizeof(neu_json_read_resp_tag_t));
    if (NULL == json->tags) {
        return -1;
    }

    for (int i = 0; i < json->n_tag; i++) {
        neu_resp_tag_value_t *tag  = &tags[i];
        neu_type_e            type = tag->value.type;
        json->tags[i].name         = tag->tag;
        json->tags[i].error        = NEU_ERR_SUCCESS;

        switch (type) {
        case NEU_TYPE_ERROR:
            json->tags[i].t             = NEU_JSON_INT;
            json->tags[i].value.val_int = tag->value.value.i32;
            json->tags[i].error         = tag->value.value.i32;
            break;
        case NEU_TYPE_UINT8:
            json->tags[i].t             = NEU_JSON_INT;
            json->tags[i].value.val_int = tag->value.value.u8;
            break;
        case NEU_TYPE_INT8:
            json->tags[i].t             = NEU_JSON_INT;
            json->tags[i].value.val_int = tag->value.value.i8;
            break;
        case NEU_TYPE_INT16:
            json->tags[i].t             = NEU_JSON_INT;
            json->tags[i].value.val_int = tag->value.value.i16;
            break;
        case NEU_TYPE_INT32:
            json->tags[i].t             = NEU_JSON_INT;
            json->tags[i].value.val_int = tag->value.value.i32;
            break;
        case NEU_TYPE_INT64:
            json->tags[i].t             = NEU_JSON_INT;
            json->tags[i].value.val_int = tag->value.value.i64;
            break;
        case NEU_TYPE_WORD:
        case NEU_TYPE_UINT16:
            json->tags[i].t             = NEU_JSON_INT;
            json->tags[i].value.val_int = tag->value.value.u16;
            break;
        case NEU_TYPE_DWORD:
        case NEU_TYPE_UINT32:
            json->tags[i].t             = NEU_JSON_INT;
            json->tags[i].value.val_int = tag->value.value.u32;
            break;
        case NEU_TYPE_LWORD:
        case NEU_TYPE_UINT64:
            json->tags[i].t             = NEU_JSON_INT;
            json->tags[i].value.val_int = tag->value.value.u64;
            break;
        case NEU_TYPE_FLOAT:
            json->tags[i].t               = NEU_JSON_FLOAT;
            json->tags[i].value.val_float = tag->value.value.f32;
            json->tags[i].precision       = tag->value.precision;
            break;
        case NEU_TYPE_DOUBLE:
            json->tags[i].t                = NEU_JSON_DOUBLE;
            json->tags[i].value.val_double = tag->value.value.d64;
            json->tags[i].precision        = tag->value.precision;
            break;
        case NEU_TYPE_BOOL:
            json->tags[i].t              = NEU_JSON_BOOL;
            json->tags[i].value.val_bool = tag->value.value.boolean;
            break;
        case NEU_TYPE_BIT:
            json->tags[i].t             = NEU_JSON_BIT;
            json->tags[i].value.val_bit = tag->value.value.u8;
            break;
        case NEU_TYPE_STRING:
            json->tags[i].t             = NEU_JSON_STR;
            json->tags[i].value.val_str = tag->value.value.str;
            break;
        default:
            break;
        }
    }

    return 0;
}

static char *generate_upload_json(neu_plugin_t *            plugin,
                                  neu_reqresp_trans_data_t *data,
                                  mqtt_upload_format_e      format)
{
    neu_resp_tag_value_t *   tags     = data->tags;
    uint16_t                 len      = data->n_tag;
    char *                   json_str = NULL;
    neu_json_read_periodic_t header   = { .group     = (char *) data->group,
                                        .node      = (char *) data->driver,
                                        .timestamp = global_timestamp };
    neu_json_read_resp_t     json     = { 0 };

    if (0 != tag_values_to_json(tags, len, &json)) {
        plog_error(plugin, "tag_values_to_json fail");
        return NULL;
    }

    if (MQTT_UPLOAD_FORMAT_VALUES == format) { // values
        neu_json_encode_with_mqtt(&json, neu_json_encode_read_resp1, &header,
                                  neu_json_encode_read_periodic_resp,
                                  &json_str);
    } else if (MQTT_UPLOAD_FORMAT_TAGS == format) { // tags
        neu_json_encode_with_mqtt(&json, neu_json_encode_read_resp, &header,
                                  neu_json_encode_read_periodic_resp,
                                  &json_str);
    } else {
        plog_warn(plugin, "invalid upload format: %d", format);
    }

    if (json.tags) {
        free(json.tags);
    }
    return json_str;
}

static char *generate_read_resp_json(neu_plugin_t *         plugin,
                                     neu_json_mqtt_t *      mqtt,
                                     neu_resp_read_group_t *data)
{
    neu_resp_tag_value_t *tags     = data->tags;
    uint16_t              len      = data->n_tag;
    char *                json_str = NULL;
    neu_json_read_resp_t  json     = { 0 };

    if (0 != tag_values_to_json(tags, len, &json)) {
        plog_error(plugin, "tag_values_to_json fail");
        return NULL;
    }

    neu_json_encode_with_mqtt(&json, neu_json_encode_read_resp, mqtt,
                              neu_json_encode_mqtt_resp, &json_str);

    if (json.tags) {
        free(json.tags);
    }
    return json_str;
}

static char *generate_write_resp_json(neu_plugin_t *    plugin,
                                      neu_json_mqtt_t * mqtt,
                                      neu_resp_error_t *data)
{
    (void) plugin;

    neu_json_error_resp_t error    = { .error = data->error };
    char *                json_str = NULL;

    neu_json_encode_with_mqtt(&error, neu_json_encode_error_resp, mqtt,
                              neu_json_encode_mqtt_resp, &json_str);

    return json_str;
}

static inline int send_read_req(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt,
                                neu_json_read_req_t *req)
{
    plog_info(plugin, "read uuid:%s, group:%s, node:%s", mqtt->uuid, req->group,
              req->node);

    neu_reqresp_head_t header = { 0 };
    header.ctx                = mqtt;
    header.type               = NEU_REQ_READ_GROUP;
    neu_req_read_group_t cmd  = { 0 };
    strcpy(cmd.driver, req->node);
    strcpy(cmd.group, req->group);
    if (0 != neu_plugin_op(plugin, header, &cmd)) {
        plog_error(plugin, "neu_plugin_op(NEU_REQ_READ_GROUP) fail");
        return -1;
    }

    return 0;
}

static int send_write_req(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt,
                          neu_json_write_req_t *req)
{
    plog_info(plugin, "write uuid:%s, group:%s, node:%s", mqtt->uuid,
              req->group, req->node);

    neu_reqresp_head_t  header = { 0 };
    neu_req_write_tag_t cmd    = { 0 };

    header.ctx  = mqtt;
    header.type = NEU_REQ_WRITE_TAG;

    strcpy(cmd.driver, req->node);
    strcpy(cmd.group, req->group);
    strcpy(cmd.tag, req->tag);

    switch (req->t) {
    case NEU_JSON_INT:
        cmd.value.type      = NEU_TYPE_INT64;
        cmd.value.value.u64 = req->value.val_int;
        break;
    case NEU_JSON_STR:
        cmd.value.type = NEU_TYPE_STRING;
        strcpy(cmd.value.value.str, req->value.val_str);
        break;
    case NEU_JSON_DOUBLE:
        cmd.value.type      = NEU_TYPE_DOUBLE;
        cmd.value.value.d64 = req->value.val_double;
        break;
    case NEU_JSON_BOOL:
        cmd.value.type          = NEU_TYPE_BOOL;
        cmd.value.value.boolean = req->value.val_bool;
        break;
    default:
        plog_error(plugin, "invalid tag value type: %d", req->t);
        return -1;
    }

    if (0 != neu_plugin_op(plugin, header, &cmd)) {
        plog_error(plugin, "neu_plugin_op(NEU_REQ_WRITE_TAG) fail");
        return -1;
    }

    return 0;
}

static void publish_cb(int errcode, neu_mqtt_qos_e qos, char *topic,
                       uint8_t *payload, uint32_t len, void *data)
{
    (void) qos;
    (void) topic;
    (void) len;

    neu_plugin_t *plugin = data;

    neu_adapter_update_metric_cb_t update_metric =
        plugin->common.adapter_callbacks->update_metric;

    if (0 == errcode) {
        update_metric(plugin->common.adapter, NEU_METRIC_SEND_MSGS_TOTAL, 1,
                      NULL);
    } else {
        update_metric(plugin->common.adapter, NEU_METRIC_SEND_MSG_ERRORS_TOTAL,
                      1, NULL);
    }

    free(payload);
}

static inline int publish(neu_plugin_t *plugin, neu_mqtt_qos_e qos, char *topic,
                          char *payload, size_t payload_len)
{
    neu_adapter_update_metric_cb_t update_metric =
        plugin->common.adapter_callbacks->update_metric;

    int rv =
        neu_mqtt_client_publish(plugin->client, qos, topic, (uint8_t *) payload,
                                (uint32_t) payload_len, plugin, publish_cb);
    if (0 != rv) {
        plog_error(plugin, "pub [%s, QoS%d] fail", topic, qos);
        update_metric(plugin->common.adapter, NEU_METRIC_SEND_MSG_ERRORS_TOTAL,
                      1, NULL);
        free(payload);
        rv = NEU_ERR_MQTT_PUBLISH_FAILURE;
    }

    return rv;
}

void handle_write_req(neu_mqtt_qos_e qos, const char *topic,
                      const uint8_t *payload, uint32_t len, void *data)
{
    int                   rv     = 0;
    neu_plugin_t *        plugin = data;
    neu_json_write_req_t *req    = NULL;

    (void) qos;

    neu_adapter_update_metric_cb_t update_metric =
        plugin->common.adapter_callbacks->update_metric;
    update_metric(plugin->common.adapter, NEU_METRIC_RECV_MSGS_TOTAL, 1, NULL);

    char *json_str = malloc(len + 1);
    if (NULL == json_str) {
        return;
    }

    memcpy(json_str, payload, len);
    json_str[len] = '\0';

    neu_json_mqtt_t *mqtt = NULL;
    rv                    = neu_json_decode_mqtt_req(json_str, &mqtt);
    if (0 != rv) {
        plog_error(plugin, "neu_json_decode_mqtt_req failed");
        free(json_str);
        return;
    }

    rv = neu_json_decode_write_req(json_str, &req);
    if (0 != rv) {
        plog_error(plugin, "neu_json_decode_write_req fail");
        neu_json_decode_mqtt_req_free(mqtt);
        free(json_str);
        return;
    }
    mqtt->topic = (char *) topic;
    rv          = send_write_req(plugin, mqtt, req);
    if (0 != rv) {
        neu_json_decode_mqtt_req_free(mqtt);
    }

    neu_json_decode_write_req_free(req);
    free(json_str);
}

int handle_write_response(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt_json,
                          neu_resp_error_t *data)
{
    int   rv       = 0;
    char *json_str = NULL;

    if (NULL == plugin->client) {
        rv = NEU_ERR_MQTT_IS_NULL;
        goto end;
    }

    if (0 == plugin->config.cache &&
        !neu_mqtt_client_is_connected(plugin->client)) {
        // cache disable and we are disconnected
        rv = NEU_ERR_MQTT_FAILURE;
        goto end;
    }

    json_str = generate_write_resp_json(plugin, mqtt_json, data);
    if (NULL == json_str) {
        plog_error(plugin, "generate write resp json fail, uuid:%s",
                   mqtt_json->uuid);
        rv = NEU_ERR_EINTERNAL;
        goto end;
    }

    char *topic = NULL;
    neu_asprintf(&topic, "%s/resp", mqtt_json->topic);
    neu_mqtt_qos_e qos = plugin->config.qos;
    rv       = publish(plugin, qos, topic, json_str, strlen(json_str));
    json_str = NULL;

end:
    neu_json_decode_mqtt_req_free(mqtt_json);
    return rv;
}

void handle_read_req(neu_mqtt_qos_e qos, const char *topic,
                     const uint8_t *payload, uint32_t len, void *data)
{
    int                  rv     = 0;
    neu_plugin_t *       plugin = data;
    neu_json_read_req_t *req    = NULL;

    (void) qos;
    (void) topic;

    neu_adapter_update_metric_cb_t update_metric =
        plugin->common.adapter_callbacks->update_metric;
    update_metric(plugin->common.adapter, NEU_METRIC_RECV_MSGS_TOTAL, 1, NULL);

    char *json_str = malloc(len + 1);
    if (NULL == json_str) {
        return;
    }

    memcpy(json_str, payload, len);
    json_str[len] = '\0';

    neu_json_mqtt_t *mqtt = NULL;
    rv                    = neu_json_decode_mqtt_req(json_str, &mqtt);
    if (0 != rv) {
        plog_error(plugin, "neu_json_decode_mqtt_req failed");
        free(json_str);
        return;
    }

    rv = neu_json_decode_read_req(json_str, &req);
    if (0 != rv) {
        plog_error(plugin, "neu_json_decode_read_req fail");
        neu_json_decode_mqtt_req_free(mqtt);
        free(json_str);
        return;
    }

    rv = send_read_req(plugin, mqtt, req);
    if (0 != rv) {
        neu_json_decode_mqtt_req_free(mqtt);
    }

    neu_json_decode_read_req_free(req);
    free(json_str);
}

int handle_read_response(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt_json,
                         neu_resp_read_group_t *data)
{
    int   rv       = 0;
    char *json_str = NULL;

    if (NULL == plugin->client) {
        rv = NEU_ERR_MQTT_IS_NULL;
        goto end;
    }

    if (0 == plugin->config.cache &&
        !neu_mqtt_client_is_connected(plugin->client)) {
        // cache disable and we are disconnected
        rv = NEU_ERR_MQTT_FAILURE;
        goto end;
    }

    json_str = generate_read_resp_json(plugin, mqtt_json, data);
    if (NULL == json_str) {
        plog_error(plugin, "generate read resp json fail");
        rv = NEU_ERR_EINTERNAL;
        goto end;
    }

    char *         topic = plugin->read_resp_topic;
    neu_mqtt_qos_e qos   = plugin->config.qos;
    rv       = publish(plugin, qos, topic, json_str, strlen(json_str));
    json_str = NULL;

end:
    neu_json_decode_mqtt_req_free(mqtt_json);
    free(data->tags);
    return rv;
}

int handle_trans_data(neu_plugin_t *            plugin,
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

    const route_entry_t *route = route_tbl_get(
        &plugin->route_tbl, trans_data->driver, trans_data->group);
    if (NULL == route) {
        plog_error(plugin, "no route for driver:%s group:%s",
                   trans_data->driver, trans_data->group);
        return NEU_ERR_GROUP_NOT_SUBSCRIBE;
    }

    char *json_str =
        generate_upload_json(plugin, trans_data, plugin->config.format);
    if (NULL == json_str) {
        plog_error(plugin, "generate upload json fail");
        return NEU_ERR_EINTERNAL;
    }

    char *         topic = route->topic;
    neu_mqtt_qos_e qos   = plugin->config.qos;
    rv       = publish(plugin, qos, topic, json_str, strlen(json_str));
    json_str = NULL;

    return rv;
}

static inline char *default_upload_topic(neu_req_subscribe_t *info)
{
    char *t = NULL;
    neu_asprintf(&t, "/neuron/%s/%s/%s", info->app, info->driver, info->group);
    return t;
}

int handle_subscribe_group(neu_plugin_t *plugin, neu_req_subscribe_t *sub_info)
{
    int rv = 0;

    neu_json_elem_t topic = { .name = "topic", .t = NEU_JSON_STR };
    if (NULL == sub_info->params) {
        // no parameters, try default topic
        topic.v.val_str = default_upload_topic(sub_info);
        if (NULL == topic.v.val_str) {
            goto end;
        }
    } else if (0 != neu_parse_param(sub_info->params, NULL, 1, &topic)) {
        plog_error(plugin, "parse `%s` for topic fail", sub_info->params);
        rv = NEU_ERR_GROUP_PARAMETER_INVALID;
        goto end;
    }

    rv = route_tbl_add_new(&plugin->route_tbl, sub_info->driver,
                           sub_info->group, topic.v.val_str);
    // topic.v.val_str ownership moved
    if (0 != rv) {
        plog_error(plugin, "route driver:%s group:%s to topic:%s fail, `%s`",
                   sub_info->driver, sub_info->group, topic.v.val_str,
                   sub_info->params);
        goto end;
    }

    plog_info(plugin, "route driver:%s group:%s to topic:%s", sub_info->driver,
              sub_info->group, topic.v.val_str);

end:
    free(sub_info->params);
    return rv;
}

int handle_unsubscribe_group(neu_plugin_t *         plugin,
                             neu_req_unsubscribe_t *unsub_info)
{
    route_tbl_del(&plugin->route_tbl, unsub_info->driver, unsub_info->group);
    plog_info(plugin, "del route driver:%s group:%s", unsub_info->driver,
              unsub_info->group);
    return 0;
}
