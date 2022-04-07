/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2021 EMQ Technologies Co., Ltd All rights reserved.
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

#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/queue.h>

#include <nng/nng.h>

#include "connection/neu_tcp.h"
#include "data_expr.h"
#include "datatag_table.h"
#include "neu_vector.h"
#include "neuron.h"
#include "tag_group_config.h"
#include "json/neu_json_param.h"

#include "modbus.h"
#include "modbus_point.h"

const neu_plugin_module_t neu_plugin_module;

struct subscribe_instance {
    neu_taggrp_config_t *   grp_configs;
    char *                  name;
    modbus_point_context_t *point_ctx;
    nng_aio *               aio;
    neu_plugin_t *          plugin;

    TAILQ_ENTRY(subscribe_instance) node;
};

struct neu_plugin {
    neu_plugin_common_t  common;
    pthread_mutex_t      mtx;
    neu_tcp_client_t *   client;
    neu_node_id_t        node_id;
    neu_datatag_table_t *tag_table;
    TAILQ_HEAD(, subscribe_instance) sub_instances;
};

static void    start_periodic_read(neu_plugin_t *       plugin,
                                   neu_taggrp_config_t *grp_config);
static void    stop_periodic_read(neu_plugin_t *       plugin,
                                  neu_taggrp_config_t *grp_config);
static void    tags_periodic_read(nng_aio *aio, void *arg, int code);
static ssize_t send_recv_callback(void *arg, char *send_buf, ssize_t send_len,
                                  char *recv_buf, ssize_t recv_len);

static ssize_t send_recv_callback(void *arg, char *send_buf, ssize_t send_len,
                                  char *recv_buf, ssize_t recv_len)
{
    neu_plugin_t *plugin = (neu_plugin_t *) arg;

    if (neu_tcp_client_is_connected(plugin->client)) {
        plugin->common.link_state = NEU_PLUGIN_LINK_STATE_CONNECTED;
    } else {
        plugin->common.link_state = NEU_PLUGIN_LINK_STATE_CONNECTING;
    }
    return neu_tcp_client_send_recv(plugin->client, send_buf, send_len,
                                    recv_buf, recv_len);
}

static void start_periodic_read(neu_plugin_t *       plugin,
                                neu_taggrp_config_t *grp_config)
{

    struct subscribe_instance *sub_inst =
        calloc(1, sizeof(struct subscribe_instance));
    vector_t *ids      = neu_taggrp_cfg_get_datatag_ids(grp_config);
    nng_aio * aio      = NULL;
    uint32_t  interval = neu_taggrp_cfg_get_interval(grp_config);

    nng_aio_alloc(&aio, NULL, NULL);
    nng_aio_set_timeout(aio, interval != 0 ? interval : 10000);

    sub_inst->grp_configs =
        (neu_taggrp_config_t *) neu_taggrp_cfg_ref(grp_config);
    sub_inst->point_ctx = modbus_point_init(plugin);
    sub_inst->aio       = aio;
    sub_inst->plugin    = plugin;
    sub_inst->name      = strdup(neu_taggrp_cfg_get_name(grp_config));

    VECTOR_FOR_EACH(ids, iter)
    {
        neu_datatag_id_t *id  = (neu_datatag_id_t *) iterator_get(&iter);
        neu_datatag_t *   tag = neu_datatag_tbl_get(plugin->tag_table, *id);

        if (tag == NULL) {
            // The tag had been deleted by other node
            continue;
        }

        if ((tag->attribute & NEU_ATTRIBUTE_READ) == NEU_ATTRIBUTE_READ) {
            switch (tag->type) {
            case NEU_DTYPE_UINT16:
            case NEU_DTYPE_INT16:
                modbus_point_add(sub_inst->point_ctx, tag->addr_str,
                                 MODBUS_B16);
                break;
            case NEU_DTYPE_FLOAT:
            case NEU_DTYPE_UINT32:
            case NEU_DTYPE_INT32:
                modbus_point_add(sub_inst->point_ctx, tag->addr_str,
                                 MODBUS_B32);
                break;
            case NEU_DTYPE_BIT:
                modbus_point_add(sub_inst->point_ctx, tag->addr_str, MODBUS_B8);
                break;
            default:
                break;
            }
        }
    }

    modbus_point_new_cmd(sub_inst->point_ctx);

    pthread_mutex_lock(&plugin->mtx);
    TAILQ_INSERT_TAIL(&plugin->sub_instances, sub_inst, node);
    pthread_mutex_unlock(&plugin->mtx);

    log_info("start periodic read, grp: %s(%p), interval: %d", sub_inst->name,
             grp_config, interval);
    nng_aio_defer(aio, tags_periodic_read, sub_inst);
}

static void free_sub_instance(struct subscribe_instance *sub_inst)
{
    modbus_point_destory(sub_inst->point_ctx);
    neu_taggrp_cfg_free(sub_inst->grp_configs);
    free(sub_inst->name);
    free(sub_inst);
}

static void stop_periodic_read(neu_plugin_t *       plugin,
                               neu_taggrp_config_t *grp_config)
{
    struct subscribe_instance *sub_inst = NULL;

    pthread_mutex_lock(&plugin->mtx);
    TAILQ_FOREACH(sub_inst, &plugin->sub_instances, node)
    {
        if (sub_inst->grp_configs == grp_config) {
            TAILQ_REMOVE(&plugin->sub_instances, sub_inst, node);
            nng_aio_cancel(sub_inst->aio);
            log_info("stop periodic read, grp: %p", grp_config);
            break;
        }
    }
    pthread_mutex_unlock(&plugin->mtx);
}

static neu_data_val_t *
setup_read_resp_data_value(neu_datatag_table_t *   tag_table,
                           neu_taggrp_config_t *   grp_config,
                           modbus_point_context_t *point_ctx)
{
    vector_t *      ids      = neu_taggrp_cfg_get_datatag_ids(grp_config);
    neu_data_val_t *resp_val = neu_datatag_pack_create(ids->size);
    int             index    = -1;

    VECTOR_FOR_EACH(ids, iter_id)
    {
        neu_datatag_id_t *id  = (neu_datatag_id_t *) iterator_get(&iter_id);
        neu_datatag_t *   tag = neu_datatag_tbl_get(tag_table, *id);
        modbus_data_t     data_tags = { 0 };

        index += 1;

        if (tag == NULL) {
            // The tag had been deleted by other node
            neu_err_code_e error = NEU_ERR_TAG_NOT_EXIST;
            neu_datatag_pack_add(resp_val, index, NEU_DTYPE_ERRORCODE, *id,
                                 (void *) &error);
            continue;
        }

        if ((tag->attribute & NEU_ATTRIBUTE_READ) != NEU_ATTRIBUTE_READ) {
            neu_err_code_e error = NEU_ERR_PLUGIN_TAG_NOT_ALLOW_READ;
            neu_datatag_pack_add(resp_val, index, NEU_DTYPE_ERRORCODE, *id,
                                 (void *) &error);
            continue;
        }

        switch (tag->type) {
        case NEU_DTYPE_UINT32:
        case NEU_DTYPE_INT32:
        case NEU_DTYPE_FLOAT:
            data_tags.type = MODBUS_B32;
            break;
        case NEU_DTYPE_UINT16:
        case NEU_DTYPE_INT16:
            data_tags.type = MODBUS_B16;
            break;
        case NEU_DTYPE_BIT:
            data_tags.type = MODBUS_B8;
            break;
        default:
            break;
        }
        if (modbus_point_find(point_ctx, tag->addr_str, &data_tags) == -1) {
            log_error("Failed to find modbus point %s", tag->addr_str);

            neu_err_code_e error = NEU_ERR_TAG_NOT_EXIST;
            neu_datatag_pack_add(resp_val, index, NEU_DTYPE_ERRORCODE, *id,
                                 (void *) &error);
            continue;
        }

        switch (data_tags.type) {
        case MODBUS_B8: {
            neu_datatag_pack_add(resp_val, index, NEU_DTYPE_BIT, *id,
                                 (void *) &data_tags.val.val_u8);
            break;
        }
        case MODBUS_B16:
            switch (tag->type) {
            case NEU_DTYPE_INT16: {
                neu_datatag_pack_add(resp_val, index, NEU_DTYPE_INT16, *id,
                                     (void *) &data_tags.val.val_u16);
                break;
            }
            case NEU_DTYPE_UINT16: {
                neu_datatag_pack_add(resp_val, index, NEU_DTYPE_UINT16, *id,
                                     (void *) &data_tags.val.val_u16);
                break;
            }
            default:
                break;
            }
            break;
        case MODBUS_B32:
            switch (tag->type) {
            case NEU_DTYPE_INT32: {
                neu_datatag_pack_add(resp_val, index, NEU_DTYPE_INT32, *id,
                                     (void *) &data_tags.val.val_u32);
                break;
            }
            case NEU_DTYPE_UINT32: {
                neu_datatag_pack_add(resp_val, index, NEU_DTYPE_UINT32, *id,
                                     (void *) &data_tags.val.val_u32);
                break;
            }
            case NEU_DTYPE_FLOAT: {
                neu_datatag_pack_add(resp_val, index, NEU_DTYPE_FLOAT, *id,
                                     (void *) &data_tags.val.val_f32);
                break;
            }
            default:
                break;
            }
        }
    }

    return resp_val;
}

static neu_data_val_t *setup_write_resp_data_value(neu_data_val_t *write_val,
                                                   neu_plugin_t *  plugin)
{
    neu_err_code_e error = NEU_ERR_SUCCESS;

    neu_fixed_array_t *array_req = NULL;
    neu_dvalue_get_ref_array(write_val, &array_req);

    neu_data_val_t *resp_val = neu_datatag_pack_create(array_req->length);

    for (uint32_t i = 0; i < array_req->length; i++) {
        neu_int_val_t *int_val = neu_fixed_array_get(array_req, i);
        neu_dtype_e    val_type =
            neu_value_type_in_dtype(neu_dvalue_get_type(int_val->val));
        modbus_data_t  data = { 0 };
        neu_datatag_t *tag =
            neu_datatag_tbl_get(plugin->tag_table, int_val->key);

        if (tag == NULL) {
            // The tag had been deleted by other node
            error = NEU_ERR_TAG_NOT_EXIST;
            neu_datatag_pack_add(resp_val, i, NEU_DTYPE_ERRORCODE, int_val->key,
                                 (void *) &error);
            continue;
        }

        switch (val_type) {
        case NEU_DTYPE_UINT16:
            data.type = MODBUS_B16;
            neu_dvalue_get_uint16(int_val->val, &data.val.val_u16);
            error = modbus_point_write(tag->addr_str, &data, send_recv_callback,
                                       plugin);
            break;
        case NEU_DTYPE_INT16:
            data.type = MODBUS_B16;
            neu_dvalue_get_int16(int_val->val, &data.val.val_i16);
            error = modbus_point_write(tag->addr_str, &data, send_recv_callback,
                                       plugin);
            break;
        case NEU_DTYPE_INT32:
            data.type = MODBUS_B32;
            neu_dvalue_get_int32(int_val->val, &data.val.val_i32);
            error = modbus_point_write(tag->addr_str, &data, send_recv_callback,
                                       plugin);
            break;
        case NEU_DTYPE_UINT32:
            data.type = MODBUS_B32;
            neu_dvalue_get_uint32(int_val->val, &data.val.val_u32);
            error = modbus_point_write(tag->addr_str, &data, send_recv_callback,
                                       plugin);
            break;
        case NEU_DTYPE_BIT:
            data.type = MODBUS_B8;
            neu_dvalue_get_bit(int_val->val, &data.val.val_u8);
            error = modbus_point_write(tag->addr_str, &data, send_recv_callback,
                                       plugin);
            break;
        case NEU_DTYPE_FLOAT:
            data.type = MODBUS_B32;
            neu_dvalue_get_float(int_val->val, &data.val.val_f32);
            error = modbus_point_write(tag->addr_str, &data, send_recv_callback,
                                       plugin);
            break;
        default:
            error = NEU_ERR_EINTERNAL;
            break;
        }

        if (error == -1) {
            error = NEU_ERR_PLUGIN_WRITE_FAILURE;
        }

        neu_datatag_pack_add(resp_val, i, NEU_DTYPE_ERRORCODE, int_val->key,
                             (void *) &error);
    }

    return resp_val;
}

static void tags_periodic_read(nng_aio *aio, void *arg, int code)
{
    switch (code) {
    case NNG_ETIMEDOUT: {
        struct subscribe_instance *sub_inst = (struct subscribe_instance *) arg;

        if (neu_taggrp_cfg_is_anchored(sub_inst->grp_configs)) {
            neu_response_t     resp      = { 0 };
            neu_reqresp_data_t data_resp = { 0 };
            neu_data_val_t *   resp_val  = NULL;

            modbus_point_all_read(sub_inst->point_ctx, true,
                                  send_recv_callback);
            resp_val = setup_read_resp_data_value(sub_inst->plugin->tag_table,
                                                  sub_inst->grp_configs,
                                                  sub_inst->point_ctx);
            // todo modbus point find;
            data_resp.grp_config = sub_inst->grp_configs;
            data_resp.data_val   = resp_val;
            resp.req_id          = 1;
            resp.resp_type       = NEU_REQRESP_TRANS_DATA;
            resp.buf             = &data_resp;
            resp.buf_len         = sizeof(neu_reqresp_data_t);

            sub_inst->plugin->common.adapter_callbacks->response(
                sub_inst->plugin->common.adapter, &resp);

            nng_aio_defer(aio, tags_periodic_read, arg);
        } else {
            neu_taggrp_config_t *new_config = neu_system_find_group_config(
                sub_inst->plugin, sub_inst->plugin->node_id, sub_inst->name);
            neu_plugin_t *plugin = sub_inst->plugin;

            stop_periodic_read(sub_inst->plugin, sub_inst->grp_configs);

            if (new_config != NULL) {
                start_periodic_read(plugin, new_config);
            }
        }

        break;
    }
    case NNG_ECANCELED:
        free_sub_instance(arg);
        log_warn("aio: %p cancel", aio);
        nng_aio_free(aio);
        break;
    default:
        free_sub_instance(arg);
        log_warn("aio: %p, skip error: %d", aio, code);
        nng_aio_free(aio);
        break;
    }
}

static neu_plugin_t *modbus_tcp_open(neu_adapter_t *            adapter,
                                     const adapter_callbacks_t *callbacks)
{
    neu_plugin_t *plugin;

    if (adapter == NULL || callbacks == NULL) {
        log_error("Open plugin with NULL adapter or callbacks");
        return NULL;
    }

    plugin = (neu_plugin_t *) calloc(1, sizeof(neu_plugin_t));
    if (plugin == NULL) {
        log_error("Failed to allocate plugin %s",
                  neu_plugin_module.module_name);
        return NULL;
    }

    neu_plugin_common_init(&plugin->common);

    plugin->common.adapter           = adapter;
    plugin->common.adapter_callbacks = callbacks;
    plugin->common.link_state        = NEU_PLUGIN_LINK_STATE_DISCONNECTED;
    TAILQ_INIT(&plugin->sub_instances);

    return plugin;
}

static int modbus_tcp_close(neu_plugin_t *plugin)
{
    free(plugin);
    return 0;
}

static int modbus_tcp_init(neu_plugin_t *plugin)
{
    pthread_mutex_init(&plugin->mtx, NULL);
    log_info("modbus tcp init.....");

    return 0;
}

static int modbus_tcp_uninit(neu_plugin_t *plugin)
{
    struct subscribe_instance *sub_inst = NULL;

    log_info("modbus uninit start...");
    pthread_mutex_lock(&plugin->mtx);
    sub_inst = TAILQ_FIRST(&plugin->sub_instances);

    while (sub_inst != NULL) {
        TAILQ_REMOVE(&plugin->sub_instances, sub_inst, node);

        nng_aio_cancel(sub_inst->aio);

        sub_inst = TAILQ_FIRST(&plugin->sub_instances);
    }
    pthread_mutex_unlock(&plugin->mtx);

    neu_tcp_client_close(plugin->client);

    pthread_mutex_destroy(&plugin->mtx);

    log_info("modbus uninit end...");

    return 0;
}

static int modbus_tcp_config(neu_plugin_t *plugin, neu_config_t *configs)
{
    int             ret       = 0;
    char *          err_param = NULL;
    neu_json_elem_t port      = { .name = "port", .t = NEU_JSON_INT };
    neu_json_elem_t host      = { .name = "host", .t = NEU_JSON_STR };

    log_info("modbus config............. type: %d, config: %s", configs->type,
             configs->buf);

    ret = neu_parse_param(configs->buf, &err_param, 2, &port, &host);
    if (0 != ret) {
        return ret;
    }

    plugin->common.link_state = NEU_PLUGIN_LINK_STATE_CONNECTING;
    plugin->client =
        neu_tcp_client_create(plugin->client, host.v.val_str, port.v.val_int);
    if (neu_tcp_client_is_connected(plugin->client)) {
        plugin->common.link_state = NEU_PLUGIN_LINK_STATE_CONNECTED;
    }

    log_info("port = %d, host = %s,ret = %d", port.v.val_int, host.v.val_str,
             ret);

    free(host.v.val_str);
    return ret;
}

static int modbus_tcp_request(neu_plugin_t *plugin, neu_request_t *req)
{
    if (plugin->tag_table == NULL) {
        plugin->node_id = neu_plugin_self_node_id(plugin);
        plugin->tag_table =
            neu_system_get_datatags_table(plugin, plugin->node_id);
    }

    switch (req->req_type) {
    case NEU_REQRESP_READ_DATA: {
        neu_reqresp_read_t *       read_req  = (neu_reqresp_read_t *) req->buf;
        struct subscribe_instance *sub_inst  = NULL;
        neu_response_t             resp      = { 0 };
        neu_reqresp_read_resp_t    data_resp = { 0 };

        pthread_mutex_lock(&plugin->mtx);

        TAILQ_FOREACH(sub_inst, &plugin->sub_instances, node)
        {
            if (sub_inst->grp_configs == read_req->grp_config) {
                data_resp.data_val = setup_read_resp_data_value(
                    plugin->tag_table, read_req->grp_config,
                    sub_inst->point_ctx);

                log_info("find read grp: %p", read_req->grp_config);
                break;
            }
        }
        pthread_mutex_unlock(&plugin->mtx);
        if (data_resp.data_val == NULL) {
            vector_t *ids =
                neu_taggrp_cfg_get_datatag_ids(read_req->grp_config);
            data_resp.data_val = neu_datatag_pack_create(ids->size);
            int32_t error      = NEU_ERR_PLUGIN_GRP_NOT_SUBSCRIBE;
            int     index      = 0;

            VECTOR_FOR_EACH(ids, iter)
            {
                neu_datatag_id_t *id = (neu_datatag_id_t *) iterator_get(&iter);
                neu_datatag_pack_add(data_resp.data_val, index,
                                     NEU_DTYPE_ERRORCODE, *id, (void *) &error);

                index += 1;
            }
        }
        data_resp.grp_config = read_req->grp_config;
        resp.req_id          = req->req_id;
        resp.resp_type       = NEU_REQRESP_READ_RESP;
        resp.buf             = &data_resp;
        resp.buf_len         = sizeof(neu_reqresp_read_resp_t);
        resp.recver_id       = req->sender_id;

        plugin->common.adapter_callbacks->response(plugin->common.adapter,
                                                   &resp);
        break;
    }
    case NEU_REQRESP_WRITE_DATA: {
        neu_reqresp_write_t *write_req = (neu_reqresp_write_t *) req->buf;
        neu_reqresp_data_t   data_resp = { 0 };
        neu_response_t       resp      = { 0 };

        assert(req->buf_len == sizeof(neu_reqresp_write_t));

        data_resp.grp_config = write_req->grp_config;
        data_resp.data_val =
            setup_write_resp_data_value(write_req->data_val, plugin);

        memset(&resp, 0, sizeof(resp));
        resp.req_id    = req->req_id;
        resp.resp_type = NEU_REQRESP_WRITE_RESP;
        resp.buf       = &data_resp;
        resp.buf_len   = sizeof(neu_reqresp_data_t);
        resp.recver_id = req->sender_id;
        plugin->common.adapter_callbacks->response(plugin->common.adapter,
                                                   &resp);
        break;
    }
    case NEU_REQRESP_SUBSCRIBE_NODE: {
        neu_reqresp_subscribe_node_t *data =
            (neu_reqresp_subscribe_node_t *) req->buf;

        start_periodic_read(plugin, data->grp_config);
        break;
    }
    case NEU_REQRESP_UNSUBSCRIBE_NODE: {
        neu_reqresp_unsubscribe_node_t *data =
            (neu_reqresp_unsubscribe_node_t *) req->buf;

        stop_periodic_read(plugin, data->grp_config);
        break;
    }
    default:
        break;
    }

    return 0;
}

static int modbus_tcp_event_reply(neu_plugin_t *     plugin,
                                  neu_event_reply_t *reqply)
{
    (void) plugin;
    (void) reqply;
    return 0;
}

static int modbus_tcp_start(neu_plugin_t *plugin)
{
    (void) plugin;
    return 0;
}

static int modbus_tcp_stop(neu_plugin_t *plugin)
{
    (void) plugin;
    return 0;
}

static int modbus_tcp_validate_tag(neu_plugin_t *plugin, neu_datatag_t *tag)
{
    (void) plugin;
    if ((tag->attribute & 0xfffc) > 0) {
        return NEU_ERR_TAG_ATTRIBUTE_NOT_SUPPORT;
    }

    if (tag->type != NEU_DTYPE_INT16 && tag->type != NEU_DTYPE_UINT16 &&
        tag->type != NEU_DTYPE_INT32 && tag->type != NEU_DTYPE_UINT32 &&
        tag->type != NEU_DTYPE_FLOAT && tag->type != NEU_DTYPE_BOOL &&
        tag->type != NEU_DTYPE_BIT && tag->type != NEU_DTYPE_INT8 &&
        tag->type != NEU_DTYPE_UINT8) {
        return NEU_ERR_TAG_TYPE_NOT_SUPPORT;
    }

    int a, b;
    int n = sscanf(tag->addr_str, "%d!%d", &a, &b);
    if (n != 2) {
        return NEU_ERR_TAG_ADDRESS_FORMAT_INVALID;
    }

    return NEU_ERR_SUCCESS;
}

static const neu_plugin_intf_funs_t plugin_intf_funs = {
    .open         = modbus_tcp_open,
    .close        = modbus_tcp_close,
    .init         = modbus_tcp_init,
    .uninit       = modbus_tcp_uninit,
    .start        = modbus_tcp_start,
    .stop         = modbus_tcp_stop,
    .config       = modbus_tcp_config,
    .request      = modbus_tcp_request,
    .event_reply  = modbus_tcp_event_reply,
    .validate_tag = modbus_tcp_validate_tag,
};

const neu_plugin_module_t neu_plugin_module = {
    .version      = NEURON_PLUGIN_VER_1_0,
    .module_name  = "modbus-tcp",
    .module_descr = "modbus tcp driver plugin",
    .intf_funs    = &plugin_intf_funs,
    .kind         = PLUGIN_KIND_SYSTEM,
    .type         = NEU_NODE_TYPE_DRIVER,
};
