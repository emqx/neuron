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
#if defined(__APPLE__)
#else
#include <sys/epoll.h>
#include <sys/queue.h>
#include <sys/timerfd.h>
#endif

#include "connection/neu_tcp.h"
#include "neu_datatag_table.h"
#include "neu_tag_group_config.h"
#include "neu_vector.h"
#include "neuron.h"

#include "modbus.h"

const neu_plugin_module_t neu_plugin_module;

enum process_status {
    STOP    = 0,
    RUNNING = 1,
};

enum device_connect_status {
    DISCONNECTED = 0,
    CONNECTED    = 1,
};

struct neu_plugin {
    neu_plugin_common_t        common;
    int                        epoll_fd;
    int                        timer_fd;
    pthread_mutex_t            mtx;
    pthread_t                  loop;
    neu_tcp_client_t *         client;
    enum process_status        status;
    enum device_connect_status connect_status;
    neu_datatag_table_t *      tag_table;
    uint16_t                   point_size;
    TAILQ_HEAD(, modbus_point) point_list;
};

static void *loop(void *arg);
static void  del_timer(neu_plugin_t *plugin);
static void  add_timer(neu_plugin_t *plugin, uint32_t interval);
static void  insert_point(neu_plugin_t *plugin, modbus_point_t *point);
static void  clean_point(neu_plugin_t *plugin);
static void  pre_process_point(neu_plugin_t *plugin);
static int   send_recv_reqrsp(neu_plugin_t *plugin);

static int send_recv_reqrsp(neu_plugin_t *plugin)
{
    modbus_point_t *point          = NULL;
    modbus_point_t *first_point    = NULL;
    char            send_buf[1500] = { 0 };
    char            recv_buf[1500] = { 0 };
    ssize_t         send_len       = 0;
    ssize_t         recv_len       = 0;
    uint8_t         n_reg          = 0;

    pthread_mutex_lock(&plugin->mtx);
    TAILQ_FOREACH(point, &plugin->point_list, node)
    {
        switch (point->order) {
        case MODBUS_POINT_ADDR_HEAD:
            if (first_point != NULL) {
                memset(send_buf, 0, sizeof(send_buf));
                memset(recv_buf, 0, sizeof(recv_buf));

                send_len = modbus_read_req_with_head(
                    send_buf, first_point->id, first_point->device,
                    first_point->function, first_point->addr, n_reg);
                recv_len =
                    neu_tcp_client_send_recv(plugin->client, send_buf, send_len,
                                             recv_buf, sizeof(recv_buf));
                if (recv_len <= 0) {
                    log_error("recv buffer len <= 0");
                } else {
                    struct modbus_header *header =
                        (struct modbus_header *) recv_buf;
                    struct modbus_code *code =
                        (struct modbus_code *) &header[1];
                    struct modbus_pdu_read_response *pdu =
                        (struct modbus_pdu_read_response *) &code[1];
                    char *data = (char *) &pdu[1];
                    (void) pdu;
                    (void) data;

                    printf("process_no: %hd, flag: %hd, len: %hd, device: "
                           "%hhd, function: %hhd\n",
                           ntohs(header->process_no), header->flag,
                           ntohs(header->len), code->device_address,
                           code->function_code);
                }
            }
            n_reg       = 1;
            first_point = point;
            break;
        case MODBUS_POINT_ADDR_SAME:
            break;
        case MODBUS_POINT_ADDR_NEXT:
            n_reg += 1;
            break;
        }
    }
    pthread_mutex_unlock(&plugin->mtx);

    return 0;
}

static void pre_process_point(neu_plugin_t *plugin)
{
    modbus_point_t *point = NULL;
    bool            init  = true;

    pthread_mutex_lock(&plugin->mtx);

    TAILQ_FOREACH(point, &plugin->point_list, node)
    {
        modbus_point_pre_process(init, point);
        init = false;
    }

    pthread_mutex_unlock(&plugin->mtx);
}

static void clean_point(neu_plugin_t *plugin)
{
    modbus_point_t *point = NULL;

    pthread_mutex_lock(&plugin->mtx);

    point = TAILQ_FIRST(&plugin->point_list);
    while (point != NULL) {
        TAILQ_REMOVE(&plugin->point_list, point, node);
        free(point);
        point = TAILQ_FIRST(&plugin->point_list);
    }

    plugin->point_size = 0;
    pthread_mutex_unlock(&plugin->mtx);
}

static void insert_point(neu_plugin_t *plugin, modbus_point_t *point)
{
    modbus_point_t *p = NULL;

    pthread_mutex_lock(&plugin->mtx);

    if (plugin->point_size == 0) {
        plugin->point_size++;
        TAILQ_INSERT_TAIL(&plugin->point_list, point, node);
    } else {
        TAILQ_FOREACH(p, &plugin->point_list, node)
        {
            int ret = modbus_point_cmp(point, p);
            if (ret == 0) {
                plugin->point_size++;
                TAILQ_INSERT_AFTER(&plugin->point_list, p, point, node);
                break;
            } else if (ret == -1) {
                plugin->point_size++;
                TAILQ_INSERT_BEFORE(p, point, node);
                break;
            } else {
                continue;
            }
        }
    }

    pthread_mutex_unlock(&plugin->mtx);
}

#if defined(__APPLE__)
static void del_timer(neu_plugin_t *plugin)
{
    (void) plugin;
    return;
}

static void add_timer(neu_plugin_t *plugin, uint32_t interval)
{
    (void) plugin;
    (void) interval;
    return;
}

static void *loop(void *arg)
{
    neu_plugin_t *plugin = (neu_plugin_t *) arg;
    // TODO: call this function in a while loop
    send_recv_reqrsp(plugin);
    return NULL;
}

#else
static void del_timer(neu_plugin_t *plugin)
{
    if (plugin->timer_fd > 0) {
        epoll_ctl(plugin->epoll_fd, EPOLL_CTL_DEL, plugin->timer_fd, NULL);
        close(plugin->timer_fd);
        plugin->timer_fd = 0;
    }
    return;
}

static void add_timer(neu_plugin_t *plugin, uint32_t interval)
{
    struct epoll_event event = { 0 };
    int                fd    = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    struct itimerspec  tv    = { 0 };

    tv.it_interval.tv_sec  = 0;
    tv.it_interval.tv_nsec = interval * 1000 * 1000;
    tv.it_value.tv_sec     = 0;
    tv.it_value.tv_nsec    = interval * 1000 * 1000;

    event.events  = EPOLLIN;
    event.data.fd = fd;

    timerfd_settime(fd, 0, &tv, NULL);

    int ret = epoll_ctl(plugin->epoll_fd, EPOLL_CTL_ADD, fd, &event);
    log_info("add timer: %d, ret: %d, fd: %d", plugin->epoll_fd, ret, fd);

    plugin->timer_fd = fd;
}

static void *loop(void *arg)
{
    neu_plugin_t *plugin = (neu_plugin_t *) arg;

    while (1) {
        uint64_t           value;
        int                ret   = -1;
        struct epoll_event event = { 0 };

        pthread_mutex_lock(&plugin->mtx);
        if (plugin->status != RUNNING) {
            pthread_mutex_unlock(&plugin->mtx);
            break;
        }
        pthread_mutex_unlock(&plugin->mtx);

        ret = epoll_wait(plugin->epoll_fd, &event, 1, 1000);
        if (ret <= 0) {
            continue;
            log_error("epoll wait error: %d", errno);
        } else {
            switch (event.events) {
            case EPOLLIN:
                send_recv_reqrsp(plugin);
                read(event.data.fd, &value, sizeof(value));
                break;
            default:
                log_info("epoll event: %d, ret %d", event.events, ret);
                break;
            }
        }
    }

    return NULL;
}
#endif

static int           modbus_tcp_init(neu_plugin_t *plugin);
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

    plugin->common.adapter           = adapter;
    plugin->common.adapter_callbacks = callbacks;

    log_info("modbus open ......");
    // modbus_tcp_init(plugin);
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
#if defined(__APPLE__)
#else
    plugin->epoll_fd = epoll_create(1);
    plugin->timer_fd = -1;
#endif
    plugin->status = RUNNING;
    TAILQ_INIT(&plugin->point_list);

    pthread_create(&plugin->loop, NULL, loop, plugin);

    modbus_point_t *point =
        (modbus_point_t *) calloc(1, sizeof(modbus_point_t));

    if (modbus_address_parse("1!400001", point) == 0) {
        point->value.type = MODBUS_B16;
        insert_point(plugin, point);
    } else {
        log_error("modbus address parse error");
        free(point);
    }

    pre_process_point(plugin);
    add_timer(plugin, 10000);

    plugin->client = neu_tcp_client_create("192.168.50.17", 502);
    log_info("modbus tcp init.....");
    return 0;
}

static int modbus_tcp_uninit(neu_plugin_t *plugin)
{
    modbus_point_t *point = NULL;

    pthread_mutex_lock(&plugin->mtx);
    plugin->status = STOP;

    point = TAILQ_FIRST(&plugin->point_list);
    while (point != NULL) {
        TAILQ_REMOVE(&plugin->point_list, point, node);
        free(point);
        point = TAILQ_FIRST(&plugin->point_list);
    }

    pthread_mutex_unlock(&plugin->mtx);

    neu_tcp_client_close(plugin->client);

    pthread_mutex_destroy(&plugin->mtx);
#if defined(__APPLE__)
#else
    close(plugin->timer_fd);
    close(plugin->epoll_fd);
#endif

    return 0;
}

static int get_datatags_table(neu_plugin_t *plugin)
{
    int                     ret = 0;
    neu_request_t           cmd;
    neu_cmd_get_datatags_t  get_datatags_cmd;
    neu_response_t *        datatags_result = NULL;
    neu_reqresp_datatags_t *resp_datatags;

    cmd.req_type = NEU_REQRESP_GET_DATATAGS;
    cmd.req_id   = 2;
    cmd.buf      = (void *) &get_datatags_cmd;
    cmd.buf_len  = sizeof(neu_cmd_get_datatags_t);

    ret = plugin->common.adapter_callbacks->command(plugin->common.adapter,
                                                    &cmd, &datatags_result);

    if (ret < 0) {
        return -1;
    }
    resp_datatags     = (neu_reqresp_datatags_t *) datatags_result->buf;
    plugin->tag_table = resp_datatags->datatag_tbl;

    free(resp_datatags);
    free(datatags_result);
    return 0;
}

static int modbus_tcp_config(neu_plugin_t *plugin, neu_config_t *configs)
{
    (void) configs;
    int ret = 0;

    plugin->client = neu_tcp_client_create("192.168.50.17", 502);
    log_info("modbus config.............");

    ret = get_datatags_table(plugin);

    return ret;
}

static int modbus_tcp_request(neu_plugin_t *plugin, neu_request_t *req)
{
    // const adapter_callbacks_t *adapter_callbacks;
    // adapter_callbacks = plugin->common.adapter_callbacks;

    switch (req->req_type) {
    case NEU_REQRESP_READ_DATA: {
        neu_reqresp_read_t *data = (neu_reqresp_read_t *) req->buf;
        uint32_t  interval = neu_taggrp_cfg_get_interval(data->grp_config);
        vector_t *tagv     = neu_taggrp_cfg_get_datatag_ids(data->grp_config);

        del_timer(plugin);
        clean_point(plugin);

        VECTOR_FOR_EACH(tagv, iter)
        {
            modbus_point_t *point =
                (modbus_point_t *) calloc(1, sizeof(modbus_point_t));
            datatag_id_t * id  = (datatag_id_t *) iterator_get(&iter);
            neu_datatag_t *tag = neu_datatag_tbl_get(plugin->tag_table, *id);

            if (modbus_address_parse(tag->str_addr, point) == 0) {
                point->value.type = MODBUS_B16;
                insert_point(plugin, point);
            } else {
                log_error("modbus address parse error: %s", tag->str_addr);
                free(point);
            }
        }

        pre_process_point(plugin);
        add_timer(plugin, interval);

        break;
    }
    case NEU_REQRESP_WRITE_DATA: {
        neu_reqresp_write_t *data = (neu_reqresp_write_t *) req->buf;
        vector_t *tagv = neu_taggrp_cfg_get_datatag_ids(data->grp_config);

        VECTOR_FOR_EACH(tagv, iter)
        {
            datatag_id_t * id  = (datatag_id_t *) iterator_get(&iter);
            neu_datatag_t *tag = neu_datatag_tbl_get(plugin->tag_table, *id);

            (void) tag;
        }

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

static const neu_plugin_intf_funs_t plugin_intf_funs = {
    .open        = modbus_tcp_open,
    .close       = modbus_tcp_close,
    .init        = modbus_tcp_init,
    .uninit      = modbus_tcp_uninit,
    .config      = modbus_tcp_config,
    .request     = modbus_tcp_request,
    .event_reply = modbus_tcp_event_reply,
};

const neu_plugin_module_t neu_plugin_module = {
    .version      = NEURON_PLUGIN_VER_1_0,
    .module_name  = "neuron-modbus-tcp-plugin",
    .module_descr = "modbus tcp driver plugin",
    .intf_funs    = &plugin_intf_funs,
};
