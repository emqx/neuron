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

#include <librdkafka/rdkafka.h>

#include "errcodes.h"
#include "json/neu_json_fn.h"
#include "json/neu_json_rw.h"

#include "kafka_handle.h"
#include "kafka_plugin.h"

static int tag_values_to_json(UT_array *tags, neu_json_read_resp_t *json,
                              bool filter_error)
{
    int index   = 0;
    int n_valid = 0;

    if (filter_error) {
        utarray_foreach(tags, neu_resp_tag_value_meta_t *, tv)
        {
            if (tv->value.type != NEU_TYPE_ERROR) {
                n_valid += 1;
            }
        }
    } else {
        n_valid = utarray_len(tags);
    }

    if (0 == n_valid) {
        return 0;
    }

    json->n_tag = n_valid;
    json->tags  = (neu_json_read_resp_tag_t *) calloc(
        json->n_tag, sizeof(neu_json_read_resp_tag_t));
    if (NULL == json->tags) {
        return -1;
    }

    utarray_foreach(tags, neu_resp_tag_value_meta_t *, tv)
    {
        if (filter_error && tv->value.type == NEU_TYPE_ERROR) {
            continue;
        }
        neu_tag_value_to_json(tv, &json->tags[index]);
        index += 1;
    }

    return 0;
}

static char *generate_upload_json(neu_plugin_t *            plugin,
                                  neu_reqresp_trans_data_t *data, bool *skip)
{
    char *json_str = NULL;

    neu_json_read_periodic_t header = {
        .group     = (char *) data->group,
        .node      = (char *) data->driver,
        .timestamp = global_timestamp,
    };
    neu_json_read_resp_t json = { 0 };

    if (0 !=
        tag_values_to_json(data->tags, &json, !plugin->config.upload_err)) {
        plog_error(plugin, "tag_values_to_json fail");
        return NULL;
    }

    if (0 == json.n_tag) {
        *skip = true;
        return NULL;
    }

    if (plugin->config.format == KAFKA_UPLOAD_FORMAT_VALUES) {
        neu_json_encode_with_mqtt(&json, neu_json_encode_read_resp1, &header,
                                  neu_json_encode_read_periodic_resp,
                                  &json_str);
    } else {
        neu_json_encode_with_mqtt(&json, neu_json_encode_read_resp2, &header,
                                  neu_json_encode_read_periodic_resp,
                                  &json_str);
    }

    for (int i = 0; i < json.n_tag; i++) {
        if (json.tags[i].n_meta > 0) {
            free(json.tags[i].metas);
        }
    }

    if (json.tags) {
        free(json.tags);
    }
    return json_str;
}

static int kafka_produce(neu_plugin_t *plugin, const char *topic, char *payload,
                         size_t len)
{
    if (NULL == plugin->rk) {
        return -1;
    }

    rd_kafka_resp_err_t err =
        rd_kafka_producev(plugin->rk, RD_KAFKA_V_TOPIC(topic),
                          RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY),
                          RD_KAFKA_V_VALUE(payload, len),
                          RD_KAFKA_V_OPAQUE(plugin), RD_KAFKA_V_END);

    if (err) {
        if (err == RD_KAFKA_RESP_ERR__QUEUE_FULL) {
            plog_warn(plugin, "produce queue full, dropping message");
        } else {
            plog_error(plugin, "produce failed: %s", rd_kafka_err2str(err));
        }
        return -1;
    }

    rd_kafka_poll(plugin->rk, 0);
    return 0;
}

int handle_trans_data(neu_plugin_t *plugin, neu_reqresp_trans_data_t *data)
{
    int rv = 0;

    if (NULL == plugin->rk) {
        return NEU_ERR_PLUGIN_NOT_RUNNING;
    }

    const kafka_route_entry_t *route =
        kafka_route_tbl_get(&plugin->route_tbl, data->driver, data->group);

    const char *topic = route ? route->topic : plugin->config.topic;

    bool  skip     = false;
    char *json_str = generate_upload_json(plugin, data, &skip);

    if (skip) {
        return 0;
    }

    if (NULL == json_str) {
        plog_error(plugin, "generate upload json fail for driver:%s group:%s",
                   data->driver, data->group);
        return NEU_ERR_EINTERNAL;
    }

    size_t json_len = strlen(json_str);
    rv              = kafka_produce(plugin, topic, json_str, json_len);

    if (0 == rv) {
        NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_SEND_BYTES_5S,
                                 (int64_t) json_len, NULL);
        NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_SEND_BYTES_30S,
                                 (int64_t) json_len, NULL);
        NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_SEND_BYTES_60S,
                                 (int64_t) json_len, NULL);
    }

    free(json_str);
    return rv == 0 ? NEU_ERR_SUCCESS : NEU_ERR_PLUGIN_NOT_RUNNING;
}

int handle_subscribe_group(neu_plugin_t *plugin, neu_req_subscribe_t *sub)
{
    int   rv    = 0;
    char *topic = NULL;

    neu_json_elem_t el = { .name = "topic", .t = NEU_JSON_STR };
    if (NULL != sub->params &&
        0 == neu_parse_param(sub->params, NULL, 1, &el)) {
        topic = el.v.val_str;
    } else {
        topic = strdup(plugin->config.topic);
    }

    if (NULL == topic) {
        rv = NEU_ERR_EINTERNAL;
        goto end;
    }

    rv =
        kafka_route_tbl_add(&plugin->route_tbl, sub->driver, sub->group, topic);
    if (0 != rv) {
        plog_error(plugin, "route driver:%s group:%s fail", sub->driver,
                   sub->group);
        goto end;
    }

    plog_notice(plugin, "route driver:%s group:%s to topic:%s", sub->driver,
                sub->group, topic);

end:
    free(sub->params);
    free(sub->static_tags);
    return rv;
}

int handle_update_subscribe(neu_plugin_t *plugin, neu_req_subscribe_t *sub)
{
    int rv = 0;

    if (NULL == sub->params) {
        rv = NEU_ERR_GROUP_PARAMETER_INVALID;
        goto end;
    }

    neu_json_elem_t el = { .name = "topic", .t = NEU_JSON_STR };
    if (0 != neu_parse_param(sub->params, NULL, 1, &el)) {
        plog_error(plugin, "parse topic fail from `%s`", sub->params);
        rv = NEU_ERR_GROUP_PARAMETER_INVALID;
        goto end;
    }

    rv = kafka_route_tbl_update(&plugin->route_tbl, sub->driver, sub->group,
                                el.v.val_str);
    if (0 != rv) {
        plog_error(plugin, "update route driver:%s group:%s fail", sub->driver,
                   sub->group);
        goto end;
    }

    plog_notice(plugin, "update route driver:%s group:%s to topic:%s",
                sub->driver, sub->group, el.v.val_str);

end:
    free(sub->params);
    free(sub->static_tags);
    return rv;
}

int handle_unsubscribe_group(neu_plugin_t *plugin, neu_req_unsubscribe_t *unsub)
{
    kafka_route_tbl_del(&plugin->route_tbl, unsub->driver, unsub->group);
    plog_notice(plugin, "del route driver:%s group:%s", unsub->driver,
                unsub->group);
    return 0;
}

int handle_update_group(neu_plugin_t *plugin, neu_req_update_group_t *req)
{
    kafka_route_tbl_update_group(&plugin->route_tbl, req->driver, req->group,
                                 req->new_name);
    plog_notice(plugin, "update route driver:%s group:%s to %s", req->driver,
                req->group, req->new_name);
    return 0;
}

int handle_del_group(neu_plugin_t *plugin, neu_req_del_group_t *req)
{
    kafka_route_tbl_del(&plugin->route_tbl, req->driver, req->group);
    plog_notice(plugin, "del route driver:%s group:%s", req->driver,
                req->group);
    return 0;
}

int handle_update_driver(neu_plugin_t *plugin, neu_req_update_node_t *req)
{
    kafka_route_tbl_update_driver(&plugin->route_tbl, req->node, req->new_name);
    plog_notice(plugin, "update route driver:%s to %s", req->node,
                req->new_name);
    return 0;
}

int handle_del_driver(neu_plugin_t *plugin, neu_reqresp_node_deleted_t *req)
{
    kafka_route_tbl_del_driver(&plugin->route_tbl, req->node);
    plog_notice(plugin, "del route driver:%s", req->node);
    return 0;
}
