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

#define MAX_SLAVES 256

uint8_t failed_cycles[MAX_SLAVES];
bool    skip[MAX_SLAVES];

struct modbus_group_data {
    UT_array *              tags;
    char *                  group;
    modbus_read_cmd_sort_t *cmd_sort;
    modbus_address_base     address_base;
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
                            uint16_t i, uint16_t j, uint16_t *response_size,
                            uint64_t *read_tms)
{
    struct timespec t3 = { .tv_sec = plugin->retry_interval / 1000,
                           .tv_nsec =
                               1000 * 1000 * (plugin->retry_interval % 1000) };
    struct timespec t4 = { 0 };
    nanosleep(&t3, &t4);
    plog_notice(plugin, "Resend read req. Times:%hu", j + 1);
    *read_tms = neu_time_ms();
    return modbus_stack_read(
        plugin->stack, gd->cmd_sort->cmd[i].slave_id, gd->cmd_sort->cmd[i].area,
        gd->cmd_sort->cmd[i].start_address, gd->cmd_sort->cmd[i].n_register,
        response_size, false);
}

void handle_modbus_error(neu_plugin_t *plugin, struct modbus_group_data *gd,
                         uint16_t cmd_index, int error_code,
                         const char *error_message)
{
    modbus_value_handle(plugin, gd->cmd_sort->cmd[cmd_index].slave_id, 0, NULL,
                        error_code);
    if (error_message) {
        plog_error(plugin, "%s, skip, %hhu!%hu", error_message,
                   gd->cmd_sort->cmd[cmd_index].slave_id,
                   gd->cmd_sort->cmd[cmd_index].start_address);
    }
}

void finalize_modbus_read_result(neu_plugin_t *            plugin,
                                 struct modbus_group_data *gd,
                                 uint16_t cmd_index, int ret_r, int ret_buf,
                                 uint64_t read_tms, int64_t *rtt,
                                 bool *slave_err)
{
    if (ret_r <= 0) {
        handle_modbus_error(plugin, gd, cmd_index, NEU_ERR_PLUGIN_DISCONNECTED,
                            "send message failed");
        *rtt = NEU_METRIC_LAST_RTT_MS_MAX;
        neu_conn_disconnect(plugin->conn);
    } else if (ret_buf <= 0) {
        switch (ret_buf) {
        case 0:
            handle_modbus_error(plugin, gd, cmd_index,
                                NEU_ERR_PLUGIN_DEVICE_NOT_RESPONSE,
                                "no modbus response received");
            *rtt = neu_time_ms() - read_tms;
            slave_err[gd->cmd_sort->cmd[cmd_index].slave_id] = true;
            break;
        case -1:
            handle_modbus_error(plugin, gd, cmd_index,
                                NEU_ERR_PLUGIN_PROTOCOL_DECODE_FAILURE,
                                "modbus message error");
            *rtt = NEU_METRIC_LAST_RTT_MS_MAX;
            neu_conn_disconnect(plugin->conn);
            break;
        case -2:
            handle_modbus_error(plugin, gd, cmd_index,
                                NEU_ERR_PLUGIN_READ_FAILURE,
                                "modbus device response error");
            *rtt = neu_time_ms() - read_tms;
            break;
        default:
            break;
        }
    } else {
        *rtt = neu_time_ms() - read_tms;
        failed_cycles[gd->cmd_sort->cmd[cmd_index].slave_id] = 0;
    }
}

void check_modbus_read_result(neu_plugin_t *            plugin,
                              struct modbus_group_data *gd, uint16_t cmd_index,
                              int64_t *rtt, bool *slave_err)
{
    uint16_t response_size = 0;
    uint64_t read_tms      = neu_time_ms();

    int ret_r = modbus_stack_read(
        plugin->stack, gd->cmd_sort->cmd[cmd_index].slave_id,
        gd->cmd_sort->cmd[cmd_index].area,
        gd->cmd_sort->cmd[cmd_index].start_address,
        gd->cmd_sort->cmd[cmd_index].n_register, &response_size, false);

    int ret_buf = 0;
    if (ret_r > 0) {
        ret_buf = process_protocol_buf(
            plugin, gd->cmd_sort->cmd[cmd_index].slave_id, response_size);
    }

    if (ret_r <= 0 || ret_buf == 0) {
        for (uint16_t j = 0; j < plugin->max_retries; ++j) {
            ret_r = modbus_stack_read_retry(plugin, gd, cmd_index, j,
                                            &response_size, &read_tms);
            if (ret_r > 0) {
                ret_buf = process_protocol_buf(
                    plugin, gd->cmd_sort->cmd[cmd_index].slave_id,
                    response_size);
                if (ret_buf == 0) {
                    continue;
                }
            }
            break;
        }
    }

    finalize_modbus_read_result(plugin, gd, cmd_index, ret_r, ret_buf, read_tms,
                                rtt, slave_err);
}

void update_metrics_after_read(neu_plugin_t *plugin, int64_t rtt,
                               neu_plugin_group_t *group,
                               neu_conn_state_t *  state)
{
    *state = neu_conn_state(plugin->conn);
    neu_adapter_update_metric_cb_t update_metric =
        plugin->common.adapter_callbacks->update_metric;
    struct modbus_group_data *gd =
        (struct modbus_group_data *) group->user_data;

    update_metric(plugin->common.adapter, NEU_METRIC_SEND_BYTES,
                  state->send_bytes, NULL);
    update_metric(plugin->common.adapter, NEU_METRIC_RECV_BYTES,
                  state->recv_bytes, NULL);
    update_metric(plugin->common.adapter, NEU_METRIC_LAST_RTT_MS, rtt, NULL);
    update_metric(plugin->common.adapter, NEU_METRIC_GROUP_LAST_SEND_MSGS,
                  gd->cmd_sort->n_cmd, group->group_name);
}

typedef struct {
    uint8_t  slave_id;
    uint16_t degrade_time;
} degrade_timer_data_t;

void *degrade_timer(void *arg)
{
    degrade_timer_data_t *data = (degrade_timer_data_t *) arg;

    struct timespec t1 = { .tv_sec = data->degrade_time, .tv_nsec = 0 };
    struct timespec t2 = { 0 };
    nanosleep(&t1, &t2);

    skip[data->slave_id] = false;

    free(data);
    return NULL;
}

void set_skip_timer(uint8_t slave_id, uint32_t degrade_time)
{
    degrade_timer_data_t *data =
        (degrade_timer_data_t *) malloc(sizeof(degrade_timer_data_t));
    data->slave_id     = slave_id;
    data->degrade_time = degrade_time;

    failed_cycles[slave_id] = 0;

    pthread_t timer_thread;
    pthread_create(&timer_thread, NULL, degrade_timer, data);

    pthread_detach(timer_thread);
}

int modbus_group_timer(neu_plugin_t *plugin, neu_plugin_group_t *group,
                       uint16_t max_byte)
{
    neu_conn_state_t          state = { 0 };
    struct modbus_group_data *gd    = NULL;
    int64_t                   rtt   = NEU_METRIC_LAST_RTT_MS_MAX;
    struct modbus_group_data *gdt =
        (struct modbus_group_data *) group->user_data;

    if (group->user_data == NULL || gdt->address_base != plugin->address_base) {
        if (group->user_data != NULL) {
            plugin_group_free(group);
        }
        gd = calloc(1, sizeof(struct modbus_group_data));

        group->user_data  = gd;
        group->group_free = plugin_group_free;
        utarray_new(gd->tags, &ut_ptr_icd);

        utarray_foreach(group->tags, neu_datatag_t *, tag)
        {
            modbus_point_t *p = calloc(1, sizeof(modbus_point_t));
            int ret = modbus_tag_to_point(tag, p, plugin->address_base);
            if (ret != NEU_ERR_SUCCESS) {
                plog_error(plugin, "invalid tag: %s, address: %s", tag->name,
                           tag->address);
            }

            utarray_push_back(gd->tags, &p);
        }

        gd->group        = strdup(group->group_name);
        gd->cmd_sort     = modbus_tag_sort(gd->tags, max_byte);
        gd->address_base = plugin->address_base;
    }

    gd                        = (struct modbus_group_data *) group->user_data;
    plugin->plugin_group_data = gd;

    bool slave_err_record[MAX_SLAVES] = { false };

    for (uint16_t i = 0; i < gd->cmd_sort->n_cmd; i++) {
        bool    slave_err[MAX_SLAVES] = { false };
        uint8_t slave_id              = gd->cmd_sort->cmd[i].slave_id;
        plugin->cmd_idx               = i;

        if (slave_err_record[slave_id] == true) {
            continue;
        }

        if (plugin->degradation == false || skip[slave_id] == false) {
            check_modbus_read_result(plugin, gd, i, &rtt, slave_err);
        } else {
            continue;
        }

        if (plugin->degradation) {
            if (slave_err[slave_id]) {
                failed_cycles[slave_id]++;
                slave_err_record[slave_id] = true;
            }

            if (failed_cycles[slave_id] >= plugin->degrade_cycle) {
                skip[slave_id] = true;
                plog_warn(plugin, "Skip slave %hhu", slave_id);
                set_skip_timer(slave_id, plugin->degrade_time);
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

    update_metrics_after_read(plugin, rtt, group, &state);
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
                if ((*p_tag)->option.value32.is_default) {
                    modbus_convert_endianess(&dvalue.value, plugin->endianess);
                }
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
                    memset(&dvalue.value, 0, sizeof(dvalue.value));
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
    case NEU_TYPE_UINT16: {
        jtype = NEU_JSON_INT;
        uint16_t tmp_val16;
        memcpy(&tmp_val16, jvalue.val_bytes.bytes, sizeof(uint16_t));
        free(jvalue.val_bytes.bytes);
        jvalue.val_int = ntohs(tmp_val16);
        break;
    }
    case NEU_TYPE_INT16: {
        jtype = NEU_JSON_INT;
        int16_t tmp_val16;
        memcpy(&tmp_val16, jvalue.val_bytes.bytes, sizeof(int16_t));
        free(jvalue.val_bytes.bytes);
        jvalue.val_int = (int16_t) ntohs(tmp_val16);
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
    case NEU_TYPE_INT32: {
        jtype = NEU_JSON_INT;
        int32_t tmp_val32;
        memcpy(&tmp_val32, jvalue.val_bytes.bytes, sizeof(int32_t));
        free(jvalue.val_bytes.bytes);
        jvalue.val_int = (int32_t) ntohl(tmp_val32);
        break;
    }
    case NEU_TYPE_UINT32: {
        jtype = NEU_JSON_INT;
        uint32_t tmp_val32;
        memcpy(&tmp_val32, jvalue.val_bytes.bytes, sizeof(uint32_t));
        free(jvalue.val_bytes.bytes);
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
    case NEU_TYPE_INT64: {
        jtype = NEU_JSON_INT;
        int64_t tmp_val64;
        memcpy(&tmp_val64, jvalue.val_bytes.bytes, sizeof(int64_t));
        free(jvalue.val_bytes.bytes);
        jvalue.val_int = (int64_t) neu_ntohll(tmp_val64);
        break;
    }
    case NEU_TYPE_UINT64: {
        jtype = NEU_JSON_INT;
        uint64_t tmp_val64;
        memcpy(&tmp_val64, jvalue.val_bytes.bytes, sizeof(uint64_t));
        free(jvalue.val_bytes.bytes);
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

    int err = modbus_tag_to_point(&tag, &point, plugin->address_base);
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

static uint8_t convert_value(neu_plugin_t *plugin, neu_value_u *value,
                             neu_datatag_t *tag, modbus_point_t *point)
{
    uint8_t n_byte = 0;
    switch (tag->type) {
    case NEU_TYPE_UINT16:
    case NEU_TYPE_INT16:
        value->u16 = htons(value->u16);
        n_byte     = sizeof(uint16_t);
        break;
    case NEU_TYPE_FLOAT:
    case NEU_TYPE_UINT32:
    case NEU_TYPE_INT32:
        if (point->option.value32.is_default) {
            modbus_convert_endianess(value, plugin->endianess);
        }
        value->u32 = htonl(value->u32);
        n_byte     = sizeof(uint32_t);
        break;
    case NEU_TYPE_DOUBLE:
    case NEU_TYPE_INT64:
    case NEU_TYPE_UINT64:
        value->u64 = neu_htonll(value->u64);
        n_byte     = sizeof(uint64_t);
        break;
    case NEU_TYPE_BIT:
        n_byte = sizeof(uint8_t);
        break;
    case NEU_TYPE_STRING: {
        switch (point->option.string.type) {
        case NEU_DATATAG_STRING_TYPE_H:
            break;
        case NEU_DATATAG_STRING_TYPE_L:
            neu_datatag_string_ltoh(value->str, point->option.string.length);
            break;
        case NEU_DATATAG_STRING_TYPE_D:
            break;
        case NEU_DATATAG_STRING_TYPE_E:
            break;
        }
        n_byte = point->option.string.length;
        break;
    }
    case NEU_TYPE_BYTES:
        n_byte = point->option.bytes.length;
        break;
    default:
        assert(false);
        break;
    }
    return n_byte;
}

static int write_modbus_point(neu_plugin_t *plugin, void *req,
                              modbus_point_t *point, neu_value_u value,
                              uint8_t n_byte)
{
    uint16_t response_size = 0;
    int      ret           = modbus_stack_write(
        plugin->stack, req, point->slave_id, point->area, point->start_address,
        point->n_register, value.bytes.bytes, n_byte, &response_size, true);

    if (ret > 0) {
        process_protocol_buf(plugin, point->slave_id, response_size);
    }

    return ret;
}

static int write_modbus_points(neu_plugin_t *      plugin,
                               modbus_write_cmd_t *write_cmd, void *req)
{
    uint16_t response_size = 0;

    int ret = modbus_stack_write(plugin->stack, req, write_cmd->slave_id,
                                 write_cmd->area, write_cmd->start_address,
                                 write_cmd->n_register, write_cmd->bytes,
                                 write_cmd->n_byte, &response_size, false);

    if (ret > 0) {
        process_protocol_buf(plugin, write_cmd->slave_id, response_size);
    }

    return ret;
}

int modbus_write_tag(neu_plugin_t *plugin, void *req, neu_datatag_t *tag,
                     neu_value_u value)
{
    modbus_point_t point = { 0 };
    int            ret = modbus_tag_to_point(tag, &point, plugin->address_base);
    assert(ret == 0);

    uint8_t n_byte = convert_value(plugin, &value, tag, &point);
    return write_modbus_point(plugin, req, &point, value, n_byte);
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
        ret = modbus_write_tag_to_point(tag, p, plugin->address_base);
        assert(ret == 0);

        utarray_push_back(gtags->tags, &p);
    }
    gtags->cmd_sort = modbus_write_tags_sort(gtags->tags, plugin->endianess);
    for (uint16_t i = 0; i < gtags->cmd_sort->n_cmd; i++) {
        ret = write_modbus_points(plugin, &gtags->cmd_sort->cmd[i], req);
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

static ssize_t recv_data(neu_plugin_t *plugin, uint8_t *buffer, size_t size)
{
    if (plugin->is_server) {
        return neu_conn_tcp_server_recv(plugin->conn, plugin->client_fd, buffer,
                                        size);
    } else {
        return neu_conn_recv(plugin->conn, buffer, size);
    }
}

static int process_received_data(neu_plugin_t *plugin, uint8_t *recv_buf,
                                 ssize_t recv_size, uint16_t expected_size,
                                 uint8_t slave_id)
{
    if (recv_size < 512) {
        plog_recv_protocol(plugin, recv_buf, recv_size);
    }
    neu_protocol_unpack_buf_t pbuf = { 0 };
    neu_protocol_unpack_buf_init(&pbuf, recv_buf, recv_size);
    int ret = modbus_stack_recv(plugin->stack, slave_id, &pbuf);
    if (ret == MODBUS_DEVICE_ERR) {
        return -2;
    }
    return recv_size == expected_size ? ret : -1;
}

static int process_received_data_test(neu_plugin_t *plugin, uint8_t *recv_buf,
                                      ssize_t recv_size, uint16_t expected_size,
                                      void *req, modbus_point_t *point)
{
    if (recv_size < 512) {
        plog_recv_protocol(plugin, recv_buf, recv_size);
    }
    neu_protocol_unpack_buf_t pbuf = { 0 };
    neu_protocol_unpack_buf_init(&pbuf, recv_buf, recv_size);
    int ret = modbus_stack_recv_test(plugin, req, point, &pbuf);
    if (ret == MODBUS_DEVICE_ERR) {
        return -2;
    }
    return recv_size == expected_size ? ret : -1;
}

static int valid_modbus_tcp_response(neu_plugin_t *plugin, uint8_t *recv_buf,
                                     uint16_t response_size)
{
    ssize_t ret = recv_data(plugin, recv_buf, sizeof(struct modbus_header));
    if (ret <= 0) {
        return 0;
    }

    if (ret != sizeof(struct modbus_header)) {
        return -1;
    }

    struct modbus_header *header = (struct modbus_header *) recv_buf;

    if (htons(header->len) > response_size - sizeof(struct modbus_header)) {
        return -1;
    }

    ret = recv_data(plugin, recv_buf + sizeof(struct modbus_header),
                    htons(header->len));

    return ret == htons(header->len)
        ? (ssize_t)(ret + sizeof(struct modbus_header))
        : (ssize_t) -1;
}

static int process_modbus_tcp(neu_plugin_t *plugin, uint8_t *recv_buf,
                              uint16_t response_size, uint8_t slave_id)
{
    int total_recv = valid_modbus_tcp_response(plugin, recv_buf, response_size);
    if (total_recv > 0) {
        return process_received_data(plugin, recv_buf, total_recv,
                                     response_size, slave_id);
    }
    return total_recv;
}

static int process_modbus_tcp_test(neu_plugin_t *plugin, uint8_t *recv_buf,
                                   uint16_t response_size, void *req,
                                   modbus_point_t *point)
{
    int total_recv = valid_modbus_tcp_response(plugin, recv_buf, response_size);
    if (total_recv > 0) {
        return process_received_data_test(plugin, recv_buf, total_recv,
                                          response_size, req, point);
    }
    return total_recv;
}

static int process_modbus_rtu(neu_plugin_t *plugin, uint8_t *recv_buf,
                              uint16_t response_size, uint8_t slave_id)
{
    ssize_t ret = recv_data(plugin, recv_buf, response_size);
    if (ret == 0 || ret == -1) {
        return 0;
    }
    return process_received_data(plugin, recv_buf, ret, response_size,
                                 slave_id);
}

static int process_protocol_buf(neu_plugin_t *plugin, uint8_t slave_id,
                                uint16_t response_size)
{
    uint8_t *recv_buf = (uint8_t *) calloc(response_size, 1);
    if (!recv_buf) {
        return -1;
    }

    int ret = 0;
    if (plugin->protocol == MODBUS_PROTOCOL_TCP) {
        ret = process_modbus_tcp(plugin, recv_buf, response_size, slave_id);
    } else if (plugin->protocol == MODBUS_PROTOCOL_RTU) {
        ret = process_modbus_rtu(plugin, recv_buf, response_size, slave_id);
    }

    free(recv_buf);
    return ret;
}

static int process_protocol_buf_test(neu_plugin_t *plugin, void *req,
                                     modbus_point_t *point,
                                     uint16_t        response_size)
{
    uint8_t *recv_buf = (uint8_t *) calloc(response_size, 1);
    if (!recv_buf) {
        return -1;
    }

    int ret = -1;
    if (plugin->protocol == MODBUS_PROTOCOL_TCP) {
        ret = process_modbus_tcp_test(plugin, recv_buf, response_size, req,
                                      point);
    }

    free(recv_buf);
    return ret;
}