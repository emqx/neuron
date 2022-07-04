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

    plugin->common.link_state = NEU_NODE_LINK_STATE_CONNECTING;
}

int modbus_send_msg(void *ctx, uint16_t n_byte, uint8_t *bytes)
{
    neu_plugin_t *plugin = (neu_plugin_t *) ctx;

    plog_send_protocol(plugin, bytes, n_byte);
    return neu_conn_send(plugin->conn, bytes, n_byte);
}

int modbus_group_timer(neu_plugin_t *plugin, neu_plugin_group_t *group,
                       uint16_t max_byte)
{
    if (group->user_data == NULL) {
        struct modbus_group_data *gd =
            calloc(1, sizeof(struct modbus_group_data));

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
        struct modbus_group_data *gd =
            (struct modbus_group_data *) group->user_data;

        for (uint16_t i = 0; i < gd->cmd_sort->n_cmd; i++) {
            uint16_t response_size = 0;
            int      ret           = modbus_stack_read(
                plugin->stack, gd->cmd_sort->cmd[i].slave_id,
                gd->cmd_sort->cmd[i].area, gd->cmd_sort->cmd[i].start_address,
                gd->cmd_sort->cmd[i].n_register, &response_size);
            if (ret > 0) {
                plugin->plugin_group_data = gd;
                plugin->cmd_idx           = i;
                process_protocol_buf(plugin, response_size);
            }
        }
    }

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

            switch ((*p_tag)->type) {
            case NEU_TYPE_UINT16:
            case NEU_TYPE_INT16:
                if ((*p_tag)->option.value16.endian == NEU_DATATAG_ENDIAN_L16) {
                    dvalue.value.u16 = htons(dvalue.value.u16);
                }
                dvalue.type = NEU_TYPE_INT16;
                break;
            case NEU_TYPE_FLOAT: {
                uint16_t v1 = ((uint16_t *) dvalue.value.bytes)[0];
                uint16_t v2 = ((uint16_t *) dvalue.value.bytes)[1];
                uint32_t v  = 0;

                switch ((*p_tag)->option.value32.endian) {
                case NEU_DATATAG_ENDIAN_BB32:
                    v = htons(v1) << 16 | htons(v2);
                    break;
                case NEU_DATATAG_ENDIAN_BL32:
                    v = v1 << 16 | v2;
                    break;
                case NEU_DATATAG_ENDIAN_LL32:
                    v = htons(v2) << 16 | htons(v1);
                    break;
                case NEU_DATATAG_ENDIAN_LB32:
                    v = v2 << 16 | v1;
                    break;
                default:
                    assert(1 == 0);
                    break;
                }

                dvalue.type      = NEU_TYPE_FLOAT;
                dvalue.value.u32 = v;
                break;
            }
            case NEU_TYPE_INT32:
            case NEU_TYPE_UINT32: {
                uint16_t v1 = ((uint16_t *) dvalue.value.bytes)[0];
                uint16_t v2 = ((uint16_t *) dvalue.value.bytes)[1];
                uint32_t v  = 0;

                switch ((*p_tag)->option.value32.endian) {
                case NEU_DATATAG_ENDIAN_BB32:
                    v = v2 << 16 | v1;
                    break;
                case NEU_DATATAG_ENDIAN_BL32:
                    v = htons(v2) << 16 | htons(v1);
                    break;
                case NEU_DATATAG_ENDIAN_LL32:
                    v = v1 << 16 | v2;
                    break;
                case NEU_DATATAG_ENDIAN_LB32:
                    v = htons(v1) << 16 | htons(v2);
                    break;
                default:
                    assert(1 == 0);
                    break;
                }

                dvalue.type      = NEU_TYPE_UINT32;
                dvalue.value.u32 = v;

                break;
            }
            case NEU_TYPE_BIT: {
                switch ((*p_tag)->area) {
                case MODBUS_AREA_HOLD_REGISTER:
                case MODBUS_AREA_INPUT_REGISTER: {
                    neu_value16_u v16 = { 0 };
                    v16.value         = htons(*(uint16_t *) dvalue.value.bytes);
                    dvalue.value.u8 =
                        neu_value16_get_bit(v16, 15 - (*p_tag)->option.bit.bit);
                    break;
                }
                case MODBUS_AREA_COIL:
                case MODBUS_AREA_INPUT:
                    break;
                }

                dvalue.type = NEU_TYPE_BIT;
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

                dvalue.type = NEU_TYPE_STRING;
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
        if (point.option.value16.endian == NEU_DATATAG_ENDIAN_L16) {
            value.u16 = htons(value.u16);
        }
        n_byte = sizeof(uint16_t);
        break;
    }
    case NEU_TYPE_UINT32:
    case NEU_TYPE_INT32: {
        uint16_t v1 = value.u32 & 0xffff;
        uint16_t v2 = (value.u32 & 0xffff0000) >> 16;

        switch (point.option.value32.endian) {
        case NEU_DATATAG_ENDIAN_BB32:
            value.u32 = v2 << 16 | v1;
            break;
        case NEU_DATATAG_ENDIAN_BL32:
            value.u32 = htons(v2) << 16 | htons(v1);
            break;
        case NEU_DATATAG_ENDIAN_LL32:
            value.u32 = v1 << 16 | v2;
            break;
        case NEU_DATATAG_ENDIAN_LB32:
            value.u32 = htons(v1) << 16 | htons(v2);
            break;
        default:
            assert(1 == 0);
            break;
        }

        n_byte = sizeof(uint32_t);
        break;
    }
    case NEU_TYPE_BIT: {
        n_byte = sizeof(uint8_t);
        break;
    }
    case NEU_TYPE_FLOAT: {
        uint16_t v1 = value.u32 & 0xffff;
        uint16_t v2 = (value.u32 & 0xffff0000) >> 16;

        switch (point.option.value32.endian) {
        case NEU_DATATAG_ENDIAN_BB32:
            value.u32 = htons(v1) << 16 | htons(v2);
            break;
        case NEU_DATATAG_ENDIAN_BL32:
            value.u32 = v1 << 16 | v2;
            break;
        case NEU_DATATAG_ENDIAN_LL32:
            value.u32 = htons(v2) << 16 | htons(v1);
            break;
        case NEU_DATATAG_ENDIAN_LB32:
            value.u32 = v2 << 16 | v1;
            break;
        default:
            assert(1 == 0);
            break;
        }

        n_byte = sizeof(float);
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
    } else {
        plugin->common.adapter_callbacks->driver.write_response(
            plugin->common.adapter, req, NEU_ERR_PLUGIN_WRITE_FAILURE);
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
    static __thread uint8_t   recv_buf[65535] = { 0 };
    neu_protocol_unpack_buf_t pbuf            = { 0 };
    ssize_t                   ret             = 0;

    ret = neu_conn_recv(plugin->conn, recv_buf, response_size);

    if (ret > 0) {
        neu_protocol_unpack_buf_init(&pbuf, recv_buf, ret);
        plog_recv_protocol(plugin, recv_buf, ret);
        return modbus_stack_recv(plugin->stack, &pbuf);
    } else {
        return -1;
    }
}