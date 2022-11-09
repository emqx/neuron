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
#include "modbus_point.h"
#include "modbus_stack.h"

#include "modbus_req.h"

struct modbus_group_data {
    char *                  group;
    UT_array *              tags;
    modbus_read_cmd_sort_t *cmd_sort;
};

static void plugin_group_free(neu_plugin_group_t *pgp);
static int  process_protocol_buf(neu_plugin_t *plugin, uint16_t response_size);

void modbus_conn_connected(void *data, int fd)
{
    struct neu_plugin *plugin = (struct neu_plugin *) data;
    (void) fd;

    plugin->common.link_state = NEU_NODE_LINK_STATE_CONNECTED;
}

void modbus_conn_disconnected(void *data, int fd)
{
    struct neu_plugin *plugin = (struct neu_plugin *) data;
    (void) fd;

    plugin->common.link_state = NEU_NODE_LINK_STATE_DISCONNECTED;
}

int modbus_send_msg(void *ctx, uint16_t n_byte, uint8_t *bytes)
{
    neu_plugin_t *plugin = (neu_plugin_t *) ctx;

    neu_adapter_update_metric_cb_t update_metric =
        plugin->common.adapter_callbacks->update_metric;

    plog_send_protocol(plugin, bytes, n_byte);
    int ret = neu_conn_send(plugin->conn, bytes, n_byte);
    if (ret > 0) {
        update_metric(plugin->common.adapter, NEU_METRIC_SEND_BYTES, ret, NULL);
    }
    return ret;
}

int modbus_group_timer(neu_plugin_t *plugin, neu_plugin_group_t *group,
                       uint16_t max_byte)
{
    neu_adapter_update_metric_cb_t update_metric =
        plugin->common.adapter_callbacks->update_metric;

    struct modbus_group_data *gd  = NULL;
    int64_t                   rtt = NEU_METRIC_LAST_RTT_MS_MAX;

    if (group->user_data == NULL) {
        gd = calloc(1, sizeof(struct modbus_group_data));

        group->user_data  = gd;
        group->group_free = plugin_group_free;
        utarray_new(gd->tags, &ut_ptr_icd);

        utarray_foreach(group->tags, neu_datatag_t *, tag)
        {
            modbus_point_t *p   = calloc(1, sizeof(modbus_point_t));
            int             ret = modbus_tag_to_point(tag, p);
            assert(ret == 0);

            utarray_push_back(gd->tags, &p);
        }

        gd->group    = strdup(group->group_name);
        gd->cmd_sort = modbus_tag_sort(gd->tags, max_byte);
    } else {
        gd = (struct modbus_group_data *) group->user_data;
    }
    plugin->plugin_group_data = gd;

    for (uint16_t i = 0; i < gd->cmd_sort->n_cmd; i++) {
        uint16_t response_size = 0;
        int64_t  start         = neu_time_ms();
        plugin->cmd_idx        = i;

        int ret = modbus_stack_read(
            plugin->stack, gd->cmd_sort->cmd[i].slave_id,
            gd->cmd_sort->cmd[i].area, gd->cmd_sort->cmd[i].start_address,
            gd->cmd_sort->cmd[i].n_register, &response_size);
        if (ret > 0) {
            process_protocol_buf(plugin, response_size);

            int64_t end = neu_time_ms();
            if (end - start < rtt) {
                rtt = end - start;
            }
        }
    }

    update_metric(plugin->common.adapter, NEU_METRIC_LAST_RTT_MS, rtt, NULL);
    update_metric(plugin->common.adapter, NEU_METRIC_GROUP_LAST_SEND_MSGS,
                  gd->cmd_sort->n_cmd, group->group_name);

    return 0;
}

int modbus_value_handle(void *ctx, uint8_t slave_id, uint16_t n_byte,
                        uint8_t *bytes)
{
    neu_plugin_t *            plugin = (neu_plugin_t *) ctx;
    struct modbus_group_data *gd =
        (struct modbus_group_data *) plugin->plugin_group_data;
    uint16_t start_address = gd->cmd_sort->cmd[plugin->cmd_idx].start_address;
    uint16_t n_register    = gd->cmd_sort->cmd[plugin->cmd_idx].n_register;

    if (bytes == NULL) {
        neu_dvalue_t dvalue = { 0 };

        dvalue.type      = NEU_TYPE_ERROR;
        dvalue.value.i32 = NEU_ERR_PLUGIN_DISCONNECTED;
        plugin->common.adapter_callbacks->driver.update(
            plugin->common.adapter, gd->group, NULL, dvalue);
        return 0;
    }

    utarray_foreach(gd->cmd_sort->cmd[plugin->cmd_idx].tags, modbus_point_t **,
                    p_tag)
    {
        neu_dvalue_t dvalue = { 0 };

        if ((*p_tag)->start_address + (*p_tag)->n_register >
                start_address + n_register ||
            slave_id != (*p_tag)->slave_id) {
            dvalue.type      = NEU_TYPE_ERROR;
            dvalue.value.i32 = NEU_ERR_PLUGIN_READ_FAILURE;
        } else {
            switch ((*p_tag)->area) {
            case MODBUS_AREA_HOLD_REGISTER:
            case MODBUS_AREA_INPUT_REGISTER:
                if (n_byte >= ((*p_tag)->start_address - start_address) * 2 +
                        (*p_tag)->n_register * 2) {
                    memcpy(dvalue.value.bytes,
                           bytes +
                               ((*p_tag)->start_address - start_address) * 2,
                           (*p_tag)->n_register * 2);
                }
                break;
            case MODBUS_AREA_COIL:
            case MODBUS_AREA_INPUT: {
                uint16_t offset = (*p_tag)->start_address - start_address;
                if (n_byte > offset / 8) {
                    neu_value8_u u8 = { .value = bytes[offset / 8] };

                    dvalue.value.u8 = neu_value8_get_bit(u8, offset % 8);
                }
                break;
            }
            }

            dvalue.type = (*p_tag)->type;
            switch ((*p_tag)->type) {
            case NEU_TYPE_UINT16:
            case NEU_TYPE_INT16:
                dvalue.value.u16 = ntohs(dvalue.value.u16);
                break;
            case NEU_TYPE_FLOAT:
            case NEU_TYPE_INT32:
            case NEU_TYPE_UINT32:
                dvalue.value.u32 = ntohl(dvalue.value.u32);
                break;
            case NEU_TYPE_BIT: {
                switch ((*p_tag)->area) {
                case MODBUS_AREA_HOLD_REGISTER:
                case MODBUS_AREA_INPUT_REGISTER: {
                    neu_value16_u v16 = { 0 };
                    v16.value         = htons(*(uint16_t *) dvalue.value.bytes);
                    dvalue.value.u8 =
                        neu_value16_get_bit(v16, (*p_tag)->option.bit.bit);
                    break;
                }
                case MODBUS_AREA_COIL:
                case MODBUS_AREA_INPUT:
                    break;
                }

                break;
            }
            case NEU_TYPE_STRING: {
                switch ((*p_tag)->option.string.type) {
                case NEU_DATATAG_STRING_TYPE_H:
                    break;
                case NEU_DATATAG_STRING_TYPE_L:
                    neu_datatag_string_ltoh(dvalue.value.str,
                                            strlen(dvalue.value.str));
                    break;
                case NEU_DATATAG_STRING_TYPE_D:
                    break;
                case NEU_DATATAG_STRING_TYPE_E:
                    break;
                }

                if (!neu_datatag_string_is_utf8(dvalue.value.str,
                                                strlen(dvalue.value.str))) {
                    dvalue.value.str[0] = '?';
                    dvalue.value.str[1] = 0;
                }

                break;
            }
            default:
                break;
            }
        }

        plugin->common.adapter_callbacks->driver.update(
            plugin->common.adapter, gd->group, (*p_tag)->name, dvalue);
    }
    return 0;
}

int modbus_write(neu_plugin_t *plugin, void *req, neu_datatag_t *tag,
                 neu_value_u value)
{
    modbus_point_t point = { 0 };
    int            ret   = modbus_tag_to_point(tag, &point);
    assert(ret == 0);
    uint8_t n_byte = 0;

    switch (tag->type) {
    case NEU_TYPE_UINT16:
    case NEU_TYPE_INT16: {
        value.u16 = htons(value.u16);
        n_byte    = sizeof(uint16_t);
        break;
    }
    case NEU_TYPE_UINT32:
    case NEU_TYPE_INT32:
    case NEU_TYPE_FLOAT: {
        value.u32 = htonl(value.u32);
        n_byte    = sizeof(uint32_t);
        break;
    }
    case NEU_TYPE_BIT: {
        n_byte = sizeof(uint8_t);
        break;
    }
    case NEU_TYPE_STRING: {
        switch (point.option.string.type) {
        case NEU_DATATAG_STRING_TYPE_H:
            break;
        case NEU_DATATAG_STRING_TYPE_L:
            neu_datatag_string_ltoh(value.str, point.option.string.length);
            break;
        case NEU_DATATAG_STRING_TYPE_D:
            break;
        case NEU_DATATAG_STRING_TYPE_E:
            break;
        }
        n_byte = point.option.string.length;
        break;
    }
    default:
        assert(1 == 0);
        break;
    }

    uint16_t response_size = 0;
    ret = modbus_stack_write(plugin->stack, req, point.slave_id, point.area,
                             point.start_address, point.n_register, value.bytes,
                             n_byte, &response_size);
    if (ret > 0) {
        process_protocol_buf(plugin, response_size);
    }

    return 0;
}

int modbus_write_resp(void *ctx, void *req, int error)
{
    neu_plugin_t *plugin = (neu_plugin_t *) ctx;

    plugin->common.adapter_callbacks->driver.write_response(
        plugin->common.adapter, req, error);
    return 0;
}

static void plugin_group_free(neu_plugin_group_t *pgp)
{
    struct modbus_group_data *gd = (struct modbus_group_data *) pgp->user_data;

    modbus_tag_sort_free(gd->cmd_sort);

    utarray_foreach(gd->tags, modbus_point_t **, tag) { free(*tag); }

    utarray_free(gd->tags);
    free(gd->group);

    free(gd);
}

static int process_protocol_buf(neu_plugin_t *plugin, uint16_t response_size)
{
    uint8_t *                 recv_buf = calloc(response_size, 1);
    neu_protocol_unpack_buf_t pbuf     = { 0 };
    ssize_t                   ret      = 0;

    neu_adapter_update_metric_cb_t update_metric =
        plugin->common.adapter_callbacks->update_metric;

    ret = neu_conn_recv(plugin->conn, recv_buf, response_size);

    if (ret > 0) {
        update_metric(plugin->common.adapter, NEU_METRIC_RECV_BYTES, ret, NULL);
        neu_protocol_unpack_buf_init(&pbuf, recv_buf, ret);
        plog_recv_protocol(plugin, recv_buf, ret);
        ret = modbus_stack_recv(plugin->stack, &pbuf);
    }

    free(recv_buf);
    return ret;
}