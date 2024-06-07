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
#include <time.h>

#include "modbus_point.h"
#include "modbus_stack.h"

#include "modbus_req.h"

struct modbus_group_data {
    UT_array *              tags;
    char *                  group;
    modbus_read_cmd_sort_t *cmd_sort;
};

struct modbus_write_tags_data {
    UT_array *               tags;
    modbus_write_cmd_sort_t *cmd_sort;
};

static void plugin_group_free(neu_plugin_group_t *pgp);
static int  process_protocol_buf(neu_plugin_t *plugin, uint8_t slave_id,
                                 uint16_t response_size);
static int  process_protocol_buf_test(neu_plugin_t *plugin, void *req,
                                      modbus_point_t *point,
                                      uint16_t        response_size);

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

void modbus_tcp_server_listen(void *data, int fd)
{
    struct neu_plugin *  plugin = (struct neu_plugin *) data;
    neu_event_io_param_t param  = {
        .cb       = modbus_tcp_server_io_callback,
        .fd       = fd,
        .usr_data = (void *) plugin,
    };

    plugin->tcp_server_io = neu_event_add_io(plugin->events, param);
}

void modbus_tcp_server_stop(void *data, int fd)
{
    struct neu_plugin *plugin = (struct neu_plugin *) data;
    (void) fd;

    neu_event_del_io(plugin->events, plugin->tcp_server_io);
}

int modbus_tcp_server_io_callback(enum neu_event_io_type type, int fd,
                                  void *usr_data)
{
    neu_plugin_t *plugin = (neu_plugin_t *) usr_data;

    switch (type) {
    case NEU_EVENT_IO_READ: {
        int client_fd = neu_conn_tcp_server_accept(plugin->conn);
        if (client_fd > 0) {
            plugin->client_fd = client_fd;
        }

        break;
    }
    case NEU_EVENT_IO_CLOSED:
    case NEU_EVENT_IO_HUP:
        plog_warn(plugin, "tcp server recv: %d, conn closed, fd: %d", type, fd);
        neu_event_del_io(plugin->events, plugin->tcp_server_io);
        neu_conn_disconnect(plugin->conn);
        break;
    }

    return 0;
}

int modbus_send_msg(void *ctx, uint16_t n_byte, uint8_t *bytes)
{
    neu_plugin_t *plugin = (neu_plugin_t *) ctx;
    int           ret    = 0;

    plog_send_protocol(plugin, bytes, n_byte);

    if (plugin->is_server) {
        ret = neu_conn_tcp_server_send(plugin->conn, plugin->client_fd, bytes,
                                       n_byte);
    } else {
        ret = neu_conn_send(plugin->conn, bytes, n_byte);
    }

    return ret;
}

int modbus_stack_read_retry(neu_plugin_t *plugin, struct modbus_group_data *gd,
                            uint16_t i, uint16_t j, uint16_t *response_size)
{
    struct timespec t3 = { .tv_sec = plugin->retry_interval / 1000,
                           .tv_nsec =
                               1000 * 1000 * (plugin->retry_interval % 1000) };
    struct timespec t4 = { 0 };
    nanosleep(&t3, &t4);
    plog_notice(plugin, "Resend read req. Times:%hu", j + 1);
    int ret = modbus_stack_read(
        plugin->stack, gd->cmd_sort->cmd[i].slave_id, gd->cmd_sort->cmd[i].area,
        gd->cmd_sort->cmd[i].start_address, gd->cmd_sort->cmd[i].n_register,
        response_size, false);
    return ret;
}

int modbus_group_timer(neu_plugin_t *plugin, neu_plugin_group_t *group,
                       uint16_t max_byte)
{
    neu_conn_state_t               state = { 0 };
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
            if (ret != NEU_ERR_SUCCESS) {
                plog_error(plugin, "invalid tag: %s, address: %s", tag->name,
                           tag->address);
            }

            utarray_push_back(gd->tags, &p);
        }

        gd->group    = strdup(group->group_name);
        gd->cmd_sort = modbus_tag_sort(gd->tags, max_byte);
    }

    gd                        = (struct modbus_group_data *) group->user_data;
    plugin->plugin_group_data = gd;

    for (uint16_t i = 0; i < gd->cmd_sort->n_cmd; i++) {
        plugin->cmd_idx        = i;
        uint16_t response_size = 0;
        uint64_t read_tms      = neu_time_ms();
        int      ret_buf       = 0;
        int      ret_r         = modbus_stack_read(
            plugin->stack, gd->cmd_sort->cmd[i].slave_id,
            gd->cmd_sort->cmd[i].area, gd->cmd_sort->cmd[i].start_address,
            gd->cmd_sort->cmd[i].n_register, &response_size, false);
        if (ret_r > 0) {
            ret_buf = process_protocol_buf(
                plugin, gd->cmd_sort->cmd[i].slave_id, response_size);
            if (ret_buf > 0) {
                rtt = neu_time_ms() - read_tms;
            } else if (ret_buf == 0) {
                for (uint16_t j = 0; j < plugin->max_retries; j++) {
                    ret_r = modbus_stack_read_retry(plugin, gd, i, j,
                                                    &response_size);
                    if (ret_r > 0) {
                        ret_buf = process_protocol_buf(
                            plugin, gd->cmd_sort->cmd[i].slave_id,
                            response_size);
                        if (ret_buf > 0) {
                            rtt = neu_time_ms() - read_tms;
                            break;
                        } else if (ret_buf < 0) {
                            if (ret_buf == -1) {
                                modbus_value_handle(
                                    plugin, gd->cmd_sort->cmd[i].slave_id, 0,
                                    NULL,
                                    NEU_ERR_PLUGIN_PROTOCOL_DECODE_FAILURE);
                                plog_error(
                                    plugin,
                                    "modbus message error, skip, %hhu!%hu",
                                    gd->cmd_sort->cmd[i].slave_id,
                                    gd->cmd_sort->cmd[i].start_address);
                            } else if (ret_buf == -2) {
                                modbus_value_handle(
                                    plugin, gd->cmd_sort->cmd[i].slave_id, 0,
                                    NULL, NEU_ERR_PLUGIN_READ_FAILURE);
                                plog_error(
                                    plugin,
                                    "modbus device response error, skip, "
                                    "%hhu!%hu",
                                    gd->cmd_sort->cmd[i].slave_id,
                                    gd->cmd_sort->cmd[i].start_address);
                            }
                            rtt = neu_time_ms() - read_tms;
                            break;
                        }
                    } else {
                        modbus_value_handle(plugin,
                                            gd->cmd_sort->cmd[i].slave_id, 0,
                                            NULL, NEU_ERR_PLUGIN_DISCONNECTED);
                        rtt = NEU_METRIC_LAST_RTT_MS_MAX;
                        neu_conn_disconnect(plugin->conn);
                        break;
                    }
                }
                if (ret_r > 0 && ret_buf == 0) {
                    modbus_value_handle(plugin, gd->cmd_sort->cmd[i].slave_id,
                                        0, NULL,
                                        NEU_ERR_PLUGIN_DEVICE_NOT_RESPONSE);
                    plog_warn(plugin,
                              "no modbus response received, skip, %hhu!%hu",
                              gd->cmd_sort->cmd[i].slave_id,
                              gd->cmd_sort->cmd[i].start_address);
                }
            } else if (ret_buf < 0) {
                if (ret_buf == -1) {
                    modbus_value_handle(plugin, gd->cmd_sort->cmd[i].slave_id,
                                        0, NULL,
                                        NEU_ERR_PLUGIN_PROTOCOL_DECODE_FAILURE);
                    plog_error(plugin, "modbus message error, skip, %hhu!%hu",
                               gd->cmd_sort->cmd[i].slave_id,
                               gd->cmd_sort->cmd[i].start_address);
                } else if (ret_buf == -2) {
                    modbus_value_handle(plugin, gd->cmd_sort->cmd[i].slave_id,
                                        0, NULL, NEU_ERR_PLUGIN_READ_FAILURE);
                    plog_error(plugin,
                               "modbus device response error, skip, %hhu!%hu",
                               gd->cmd_sort->cmd[i].slave_id,
                               gd->cmd_sort->cmd[i].start_address);
                }
                rtt = neu_time_ms() - read_tms;
            }
        } else {
            for (uint16_t j = 0; j < plugin->max_retries; j++) {
                ret_r =
                    modbus_stack_read_retry(plugin, gd, i, j, &response_size);
                if (ret_r > 0) {
                    ret_buf = process_protocol_buf(
                        plugin, gd->cmd_sort->cmd[i].slave_id, response_size);
                    if (ret_buf > 0) {
                        rtt = neu_time_ms() - read_tms;
                        break;
                    } else if (ret_buf < 0) {
                        if (ret_buf == -1) {
                            modbus_value_handle(
                                plugin, gd->cmd_sort->cmd[i].slave_id, 0, NULL,
                                NEU_ERR_PLUGIN_PROTOCOL_DECODE_FAILURE);
                            plog_error(plugin,
                                       "modbus message error, skip, %hhu!%hu",
                                       gd->cmd_sort->cmd[i].slave_id,
                                       gd->cmd_sort->cmd[i].start_address);
                        } else if (ret_buf == -2) {
                            modbus_value_handle(
                                plugin, gd->cmd_sort->cmd[i].slave_id, 0, NULL,
                                NEU_ERR_PLUGIN_READ_FAILURE);
                            plog_error(plugin,
                                       "modbus device response error, skip, "
                                       "%hhu!%hu",
                                       gd->cmd_sort->cmd[i].slave_id,
                                       gd->cmd_sort->cmd[i].start_address);
                        }
                        rtt = neu_time_ms() - read_tms;
                        break;
                    } else if (ret_buf == 0) {
                        modbus_value_handle(
                            plugin, gd->cmd_sort->cmd[i].slave_id, 0, NULL,
                            NEU_ERR_PLUGIN_DEVICE_NOT_RESPONSE);
                        plog_warn(plugin,
                                  "no modbus response received, skip, %hhu!%hu",
                                  gd->cmd_sort->cmd[i].slave_id,
                                  gd->cmd_sort->cmd[i].start_address);
                        continue;
                    }
                }
            }
            if (ret_r <= 0) {
                modbus_value_handle(plugin, gd->cmd_sort->cmd[i].slave_id, 0,
                                    NULL, NEU_ERR_PLUGIN_DISCONNECTED);
                rtt = NEU_METRIC_LAST_RTT_MS_MAX;
                neu_conn_disconnect(plugin->conn);
                break;
            }
        }
        if (plugin->interval > 0) {
            struct timespec t1 = { .tv_sec  = plugin->interval / 1000,
                                   .tv_nsec = 1000 * 1000 *
                                       (plugin->interval % 1000) };
            struct timespec t2 = { 0 };
            nanosleep(&t1, &t2);
        }
    }

    state = neu_conn_state(plugin->conn);
    update_metric(plugin->common.adapter, NEU_METRIC_SEND_BYTES,
                  state.send_bytes, NULL);
    update_metric(plugin->common.adapter, NEU_METRIC_RECV_BYTES,
                  state.recv_bytes, NULL);
    update_metric(plugin->common.adapter, NEU_METRIC_LAST_RTT_MS, rtt, NULL);
    update_metric(plugin->common.adapter, NEU_METRIC_GROUP_LAST_SEND_MSGS,
                  gd->cmd_sort->n_cmd, group->group_name);
    return 0;
}

int modbus_value_handle(void *ctx, uint8_t slave_id, uint16_t n_byte,
                        uint8_t *bytes, int error)
{
    neu_plugin_t *            plugin = (neu_plugin_t *) ctx;
    struct modbus_group_data *gd =
        (struct modbus_group_data *) plugin->plugin_group_data;
    uint16_t start_address = gd->cmd_sort->cmd[plugin->cmd_idx].start_address;
    uint16_t n_register    = gd->cmd_sort->cmd[plugin->cmd_idx].n_register;

    if (error == NEU_ERR_PLUGIN_DISCONNECTED) {
        neu_dvalue_t dvalue = { 0 };

        dvalue.type      = NEU_TYPE_ERROR;
        dvalue.value.i32 = error;
        plugin->common.adapter_callbacks->driver.update(
            plugin->common.adapter, gd->group, NULL, dvalue);
        return 0;
    } else if (error != NEU_ERR_SUCCESS) {
        utarray_foreach(gd->cmd_sort->cmd[plugin->cmd_idx].tags,
                        modbus_point_t **, p_tag)
        {
            neu_dvalue_t dvalue = { 0 };
            dvalue.type         = NEU_TYPE_ERROR;
            dvalue.value.i32    = error;
            plugin->common.adapter_callbacks->driver.update(
                plugin->common.adapter, gd->group, (*p_tag)->name, dvalue);
        }
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
                    memcpy(dvalue.value.bytes.bytes,
                           bytes +
                               ((*p_tag)->start_address - start_address) * 2,
                           (*p_tag)->n_register * 2);
                    dvalue.value.bytes.length = (*p_tag)->n_register * 2;
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
            case NEU_TYPE_DOUBLE:
            case NEU_TYPE_INT64:
            case NEU_TYPE_UINT64:
                dvalue.value.u64 = neu_ntohll(dvalue.value.u64);
                break;
            case NEU_TYPE_BIT: {
                switch ((*p_tag)->area) {
                case MODBUS_AREA_HOLD_REGISTER:
                case MODBUS_AREA_INPUT_REGISTER: {
                    neu_value16_u v16 = { 0 };
                    v16.value = htons(*(uint16_t *) dvalue.value.bytes.bytes);
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

int modbus_value_handle_test(neu_plugin_t *plugin, void *req,
                             modbus_point_t *point, uint16_t n_byte,
                             uint8_t *bytes)
{
    (void) req;
    neu_json_value_u jvalue = { 0 };
    neu_json_type_e  jtype;

    switch (point->area) {
    case MODBUS_AREA_HOLD_REGISTER:
    case MODBUS_AREA_INPUT_REGISTER:
        if (n_byte >= point->n_register * 2) {
            jvalue.val_bytes.bytes =
                (uint8_t *) malloc(point->n_register * 2 + 1);
            memcpy(jvalue.val_bytes.bytes, bytes, point->n_register * 2);
            jvalue.val_bytes.length = point->n_register * 2;
        }
        break;
    case MODBUS_AREA_COIL:
    case MODBUS_AREA_INPUT: {
        neu_value8_u u8 = { .value = bytes[0] };
        jvalue.val_bit  = neu_value8_get_bit(u8, 0);
    } break;
    }

    switch (point->type) {
    case NEU_TYPE_UINT16:
    case NEU_TYPE_INT16: {
        jtype = NEU_JSON_INT;
        uint16_t tmp_val16;
        memcpy(&tmp_val16, jvalue.val_bytes.bytes, sizeof(uint16_t));
        free(jvalue.val_bytes.bytes);
        jvalue.val_int = ntohs(tmp_val16);
        break;
    }
    case NEU_TYPE_FLOAT: {
        uint32_t tmp_valf;
        memcpy(&tmp_valf, jvalue.val_bytes.bytes, sizeof(uint32_t));
        free(jvalue.val_bytes.bytes);
        jtype          = NEU_JSON_FLOAT;
        jvalue.val_int = ntohl(tmp_valf);
        break;
    }
    case NEU_TYPE_INT32:
    case NEU_TYPE_UINT32: {
        uint32_t tmp_val32;
        memcpy(&tmp_val32, jvalue.val_bytes.bytes, sizeof(uint32_t));
        free(jvalue.val_bytes.bytes);
        jtype          = NEU_JSON_INT;
        jvalue.val_int = ntohl(tmp_val32);
        break;
    }
    case NEU_TYPE_DOUBLE: {
        uint64_t tmp_vald;
        memcpy(&tmp_vald, jvalue.val_bytes.bytes, sizeof(uint64_t));
        free(jvalue.val_bytes.bytes);
        jtype          = NEU_JSON_DOUBLE;
        jvalue.val_int = neu_ntohll(tmp_vald);
        break;
    }
    case NEU_TYPE_INT64:
    case NEU_TYPE_UINT64: {
        uint64_t tmp_val64;
        memcpy(&tmp_val64, jvalue.val_bytes.bytes, sizeof(uint64_t));
        free(jvalue.val_bytes.bytes);
        jtype          = NEU_JSON_INT;
        jvalue.val_int = neu_ntohll(tmp_val64);
        break;
    }
    case NEU_TYPE_BIT: {
        switch (point->area) {
        case MODBUS_AREA_HOLD_REGISTER:
        case MODBUS_AREA_INPUT_REGISTER: {
            jtype             = NEU_JSON_BIT;
            neu_value16_u v16 = { 0 };
            v16.value         = htons(*(uint16_t *) jvalue.val_bytes.bytes);
            free(jvalue.val_bytes.bytes);
            jvalue.val_bit = neu_value16_get_bit(v16, point->option.bit.bit);
            break;
        }
        case MODBUS_AREA_COIL:
        case MODBUS_AREA_INPUT:
            jtype = NEU_JSON_INT;
            break;
        }
        break;
    }
    case NEU_TYPE_STRING: {
        jtype                      = NEU_JSON_STR;
        size_t str_length          = point->n_register * 2;
        jvalue.val_str             = (char *) jvalue.val_bytes.bytes;
        jvalue.val_str[str_length] = '\0';

        switch (point->option.string.type) {
        case NEU_DATATAG_STRING_TYPE_H:
            break;
        case NEU_DATATAG_STRING_TYPE_L:
            neu_datatag_string_ltoh(jvalue.val_str, str_length);
            break;
        case NEU_DATATAG_STRING_TYPE_D:
            break;
        case NEU_DATATAG_STRING_TYPE_E:
            break;
        }

        if (!neu_datatag_string_is_utf8(jvalue.val_str, str_length)) {
            jvalue.val_str[0] = '?';
            jvalue.val_str[1] = 0;
        }
        break;
    }
    default:
        break;
    }

    plugin->common.adapter_callbacks->driver.test_read_tag_response(
        plugin->common.adapter, req, jtype, point->type, jvalue,
        NEU_ERR_SUCCESS);

    return 0;
}

int modbus_test_read_tag(neu_plugin_t *plugin, void *req, neu_datatag_t tag)
{
    modbus_point_t   point = { 0 };
    neu_json_value_u error_value;
    error_value.val_int = 0;

    int err = modbus_tag_to_point(&tag, &point);
    if (err != NEU_ERR_SUCCESS) {
        plugin->common.adapter_callbacks->driver.test_read_tag_response(
            plugin->common.adapter, req, NEU_JSON_INT, NEU_TYPE_ERROR,
            error_value, err);
        return 0;
    }

    uint16_t response_size = 0;
    int      ret = modbus_stack_read(plugin->stack, point.slave_id, point.area,
                                point.start_address, point.n_register,
                                &response_size, true);
    if (ret <= 0) {
        plugin->common.adapter_callbacks->driver.test_read_tag_response(
            plugin->common.adapter, req, NEU_JSON_INT, NEU_TYPE_ERROR,
            error_value, NEU_ERR_PLUGIN_READ_FAILURE);
        return 0;
    }

    ret = process_protocol_buf_test(plugin, req, &point, response_size);
    if (ret == 0) {
        plugin->common.adapter_callbacks->driver.test_read_tag_response(
            plugin->common.adapter, req, NEU_JSON_INT, NEU_TYPE_ERROR,
            error_value, NEU_ERR_PLUGIN_DEVICE_NOT_RESPONSE);
        return 0;
    } else if (ret == -1) {
        plugin->common.adapter_callbacks->driver.test_read_tag_response(
            plugin->common.adapter, req, NEU_JSON_INT, NEU_TYPE_ERROR,
            error_value, NEU_ERR_PLUGIN_PROTOCOL_DECODE_FAILURE);
        return 0;
    } else if (ret == -2) {
        plugin->common.adapter_callbacks->driver.test_read_tag_response(
            plugin->common.adapter, req, NEU_JSON_INT, NEU_TYPE_ERROR,
            error_value, NEU_ERR_PLUGIN_READ_FAILURE);
        return 0;
    }

    return 0;
}

int modbus_write(neu_plugin_t *plugin, void *req, neu_datatag_t *tag,
                 neu_value_u value, bool response)
{
    modbus_point_t point = { 0 };
    int            ret   = modbus_tag_to_point(tag, &point);
    assert(ret == 0);
    uint8_t n_byte = 0;

    switch (tag->type) {
    case NEU_TYPE_UINT16:
    case NEU_TYPE_INT16:
        value.u16 = htons(value.u16);
        n_byte    = sizeof(uint16_t);
        break;
    case NEU_TYPE_FLOAT:
    case NEU_TYPE_UINT32:
    case NEU_TYPE_INT32:
        value.u32 = htonl(value.u32);
        n_byte    = sizeof(uint32_t);
        break;

    case NEU_TYPE_DOUBLE:
    case NEU_TYPE_INT64:
    case NEU_TYPE_UINT64:
        value.u64 = neu_htonll(value.u64);
        n_byte    = sizeof(uint64_t);
        break;
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
    case NEU_TYPE_BYTES: {
        n_byte = point.option.bytes.length;
        break;
    }
    default:
        assert(1 == 0);
        break;
    }

    uint16_t response_size = 0;
    ret                    = modbus_stack_write(
        plugin->stack, req, point.slave_id, point.area, point.start_address,
        point.n_register, value.bytes.bytes, n_byte, &response_size, response);
    if (ret > 0) {
        process_protocol_buf(plugin, point.slave_id, response_size);
    }

    return ret;
}

int modbus_write_tag(neu_plugin_t *plugin, void *req, neu_datatag_t *tag,
                     neu_value_u value)
{
    int ret = 0;
    ret     = modbus_write(plugin, req, tag, value, true);
    return ret;
}

int modbus_write_tags(neu_plugin_t *plugin, void *req, UT_array *tags)
{
    struct modbus_write_tags_data *gtags = NULL;
    int                            ret   = 0;
    int                            rv    = 0;

    gtags = calloc(1, sizeof(struct modbus_write_tags_data));

    utarray_new(gtags->tags, &ut_ptr_icd);
    utarray_foreach(tags, neu_plugin_tag_value_t *, tag)
    {
        modbus_point_write_t *p = calloc(1, sizeof(modbus_point_write_t));
        ret                     = modbus_write_tag_to_point(tag, p);
        assert(ret == 0);

        utarray_push_back(gtags->tags, &p);
    }
    gtags->cmd_sort = modbus_write_tags_sort(gtags->tags);
    for (uint16_t i = 0; i < gtags->cmd_sort->n_cmd; i++) {
        uint16_t response_size = 0;

        ret = modbus_stack_write(
            plugin->stack, req, gtags->cmd_sort->cmd[i].slave_id,
            gtags->cmd_sort->cmd[i].area, gtags->cmd_sort->cmd[i].start_address,
            gtags->cmd_sort->cmd[i].n_register, gtags->cmd_sort->cmd[i].bytes,
            gtags->cmd_sort->cmd[i].n_byte, &response_size, false);
        if (ret > 0) {
            process_protocol_buf(plugin, gtags->cmd_sort->cmd[i].slave_id,
                                 response_size);
        }

        if (ret <= 0) {
            rv = 1;
        }
        if (plugin->interval > 0) {
            struct timespec t1 = { .tv_sec  = plugin->interval / 1000,
                                   .tv_nsec = 1000 * 1000 *
                                       (plugin->interval % 1000) };
            struct timespec t2 = { 0 };
            nanosleep(&t1, &t2);
        }
    }

    if (rv == 0) {
        plugin->common.adapter_callbacks->driver.write_response(
            plugin->common.adapter, req, NEU_ERR_SUCCESS);
    } else {
        plugin->common.adapter_callbacks->driver.write_response(
            plugin->common.adapter, req, NEU_ERR_PLUGIN_DISCONNECTED);
    }

    for (uint16_t i = 0; i < gtags->cmd_sort->n_cmd; i++) {
        utarray_free(gtags->cmd_sort->cmd[i].tags);
        free(gtags->cmd_sort->cmd[i].bytes);
    }
    free(gtags->cmd_sort->cmd);
    free(gtags->cmd_sort);
    utarray_foreach(gtags->tags, modbus_point_write_t **, tag) { free(*tag); }
    utarray_free(gtags->tags);
    free(gtags);
    return ret;
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

static int process_protocol_buf(neu_plugin_t *plugin, uint8_t slave_id,
                                uint16_t response_size)
{
    uint8_t *                 recv_buf = calloc(response_size, 1);
    neu_protocol_unpack_buf_t pbuf     = { 0 };
    ssize_t                   ret      = 0;
    if (plugin->protocol == MODBUS_PROTOCOL_TCP) {
        if (plugin->is_server) {
            ret = neu_conn_tcp_server_recv(plugin->conn, plugin->client_fd,
                                           recv_buf,
                                           sizeof(struct modbus_header));
        } else {
            ret = neu_conn_recv(plugin->conn, recv_buf,
                                sizeof(struct modbus_header));
        }

        if (ret == 0 || ret == -1) {
            free(recv_buf);
            return 0;
        }

        if (ret == sizeof(struct modbus_header)) {
            struct modbus_header *header = (struct modbus_header *) recv_buf;
            int                   ret1   = 0;

            if (htons(header->len) >
                response_size - sizeof(struct modbus_header)) {
                free(recv_buf);
                return -1;
            }

            plog_recv_protocol(plugin, recv_buf, ret);
            if (plugin->is_server) {
                ret1 = neu_conn_tcp_server_recv(
                    plugin->conn, plugin->client_fd,
                    recv_buf + sizeof(struct modbus_header),
                    htons(header->len));
            } else {
                ret1 = neu_conn_recv(plugin->conn,
                                     recv_buf + sizeof(struct modbus_header),
                                     htons(header->len));
            }
            if (ret1 != htons(header->len)) {
                free(recv_buf);
                return -1;
            }
            ret += ret1;
        } else {
            free(recv_buf);
            return -1;
        }

        if (ret > 0) {
            if (ret < 512) {
                plog_recv_protocol(plugin, recv_buf, ret);
            }
            neu_protocol_unpack_buf_init(&pbuf, recv_buf, ret);
            int ret_s = modbus_stack_recv(plugin->stack, slave_id, &pbuf);
            if (ret_s == MODBUS_DEVICE_ERR) {
                ret = ret_s;
            } else {
                if (ret != response_size) {
                    ret = -1;
                } else {
                    ret = ret_s;
                }
            }
        }
    } else if (plugin->protocol == MODBUS_PROTOCOL_RTU) {
        if (plugin->is_server) {
            ret = neu_conn_tcp_server_recv(plugin->conn, plugin->client_fd,
                                           recv_buf, response_size);
        } else {
            ret = neu_conn_recv(plugin->conn, recv_buf, response_size);
        }

        if (ret == 0 || ret == -1) {
            free(recv_buf);
            return 0;
        }

        if (ret > 0) {
            if (response_size < 512) {
                plog_recv_protocol(plugin, recv_buf, ret);
            }
            neu_protocol_unpack_buf_init(&pbuf, recv_buf, ret);
            int ret_s = modbus_stack_recv(plugin->stack, slave_id, &pbuf);
            if (ret_s == MODBUS_DEVICE_ERR) {
                ret = ret_s;
            } else {
                if (ret != response_size) {
                    ret = -1;
                } else {
                    ret = ret_s;
                }
            }
        }
    }
    free(recv_buf);
    return ret;
}

static int process_protocol_buf_test(neu_plugin_t *plugin, void *req,
                                     modbus_point_t *point,
                                     uint16_t        response_size)
{
    uint8_t *                 recv_buf = calloc(response_size, 1);
    neu_protocol_unpack_buf_t pbuf     = { 0 };
    ssize_t                   ret      = 0;
    if (plugin->protocol == MODBUS_PROTOCOL_TCP) {
        if (plugin->is_server) {
            ret = neu_conn_tcp_server_recv(plugin->conn, plugin->client_fd,
                                           recv_buf,
                                           sizeof(struct modbus_header));
        } else {
            ret = neu_conn_recv(plugin->conn, recv_buf,
                                sizeof(struct modbus_header));
        }

        if (ret == 0 || ret == -1) {
            free(recv_buf);
            return 0;
        }

        if (ret == sizeof(struct modbus_header)) {
            struct modbus_header *header = (struct modbus_header *) recv_buf;
            int                   ret1   = 0;

            if (htons(header->len) >
                response_size - sizeof(struct modbus_header)) {
                free(recv_buf);
                return -1;
            }

            plog_recv_protocol(plugin, recv_buf, ret);
            if (plugin->is_server) {
                ret1 = neu_conn_tcp_server_recv(
                    plugin->conn, plugin->client_fd,
                    recv_buf + sizeof(struct modbus_header),
                    htons(header->len));
            } else {
                ret1 = neu_conn_recv(plugin->conn,
                                     recv_buf + sizeof(struct modbus_header),
                                     htons(header->len));
            }
            if (ret1 != htons(header->len)) {
                free(recv_buf);
                return -1;
            }
            ret += ret1;
        } else {
            free(recv_buf);
            return -1;
        }

        if (ret > 0) {
            if (ret < 512) {
                plog_recv_protocol(plugin, recv_buf, ret);
            }
            neu_protocol_unpack_buf_init(&pbuf, recv_buf, ret);
            int ret_s = modbus_stack_recv_test(plugin, req, point, &pbuf);
            if (ret_s == MODBUS_DEVICE_ERR) {
                ret = ret_s;
            } else {
                if (ret != response_size) {
                    ret = -1;
                } else {
                    ret = ret_s;
                }
            }
        }
    }
    free(recv_buf);
    return ret;
}