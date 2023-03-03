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

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <neuron.h>

#define UNUSED(X) ((void) X)

struct neu_plugin {
    neu_plugin_common_t common;
    neu_events_t *      udp_server_events;
    int                 udp_server_fd;
    bool                running;
    int                 port;
    int                 buf_size;
    char *              buffer;
    int                 index;
    neu_event_io_t *    event;
    pthread_mutex_t     mutex;
    /* data */
};

static neu_plugin_t *driver_open(void);

static int driver_close(neu_plugin_t *plugin);
static int driver_init(neu_plugin_t *plugin);
static int driver_uninit(neu_plugin_t *plugin);
static int driver_start(neu_plugin_t *plugin);
static int driver_stop(neu_plugin_t *plugin);
static int driver_setting(neu_plugin_t *plugin, const char *config);
static int driver_request(neu_plugin_t *plugin, neu_reqresp_head_t *head,
                          void *data);
static int driver_group_timer(neu_plugin_t *plugin, neu_plugin_group_t *group);
static int driver_validate_tag(neu_plugin_t *plugin, neu_datatag_t *tag);
static int driver_write_tag(neu_plugin_t *plugin, void *req, neu_datatag_t *tag,
                            neu_value_u value);

static int udp_server_init(neu_plugin_t *plugin);
static int udp_server_stop(neu_plugin_t *plugin);

static const neu_plugin_intf_funs_t plugin_intf_funs = {
    .open    = driver_open,
    .close   = driver_close,
    .init    = driver_init,
    .uninit  = driver_uninit,
    .start   = driver_start,
    .stop    = driver_stop,
    .setting = driver_setting,
    .request = driver_request,

    .driver.group_timer  = driver_group_timer,
    .driver.validate_tag = driver_validate_tag,
    .driver.write_tag    = driver_write_tag
};

const neu_plugin_module_t neu_plugin_module = {
    .version         = NEURON_PLUGIN_VER_2_0,
    .schema          = "udp",
    .module_name     = "udp",
    .module_descr    = "udp server",
    .module_descr_zh = "udp服务器",
    .intf_funs       = &plugin_intf_funs,
    .kind            = NEU_PLUGIN_KIND_SYSTEM,
    .type            = NEU_NA_TYPE_DRIVER,
    .display         = true,
    .single          = false,

};

static neu_plugin_t *driver_open(void)
{
    neu_plugin_t *plugin = calloc(1, sizeof(neu_plugin_t));

    if (0 != pthread_mutex_init(&plugin->mutex, NULL)) {
        free(plugin);
        return NULL;
    }

    neu_plugin_common_init(&plugin->common);

    return plugin;
}

static int driver_close(neu_plugin_t *plugin)
{
    pthread_mutex_destroy(&plugin->mutex);
    free(plugin);
    return 0;
}

static int driver_init(neu_plugin_t *plugin)
{
    plugin->udp_server_events = neu_event_new();

    plugin->running = false;

    plugin->udp_server_fd = -1;

    plugin->port     = -1;
    plugin->buf_size = 124;
    plugin->buffer   = calloc(1, plugin->buf_size);
    plugin->index    = 0;

    return 0;
}

static int driver_uninit(neu_plugin_t *plugin)
{
    udp_server_stop(plugin);
    free(plugin->buffer);
    plugin->buffer = NULL;
    neu_event_close(plugin->udp_server_events);
    plugin->udp_server_events = NULL;
    return 0;
}

static int driver_start(neu_plugin_t *plugin)
{
    if (plugin->port != -1 && !plugin->running) {
        udp_server_init(plugin);
    }
    return 0;
}

static int driver_stop(neu_plugin_t *plugin)
{
    udp_server_stop(plugin);
    return 0;
}

static int driver_setting(neu_plugin_t *plugin, const char *config)
{
    neu_json_elem_t port      = { .name = "port", .t = NEU_JSON_INT };
    neu_json_elem_t buf_size  = { .name = "buf_size", .t = NEU_JSON_INT };
    char *          err_param = NULL;

    int ret = neu_parse_param(config, &err_param, 2, &port, &buf_size);
    if (ret != 0) {
        plog_warn(plugin, "config: %s, decode error: %s", config, err_param);
        free(err_param);
        return -1;
    }

    plugin->port = port.v.val_int;

    pthread_mutex_lock(&plugin->mutex);

    if (!plugin->buffer) {
        plugin->buf_size = buf_size.v.val_int;
        plugin->buffer   = calloc(1, plugin->buf_size);
    } else if (plugin->buf_size != buf_size.v.val_int) {
        plugin->index = 0;
        free(plugin->buffer);
        plugin->buf_size = buf_size.v.val_int;
        plugin->buffer   = calloc(1, plugin->buf_size);
    }

    pthread_mutex_unlock(&plugin->mutex);

    udp_server_init(plugin);

    return 0;
}
static int driver_request(neu_plugin_t *plugin, neu_reqresp_head_t *head,
                          void *data)
{
    UNUSED(plugin);
    UNUSED(head);
    UNUSED(data);
    return 0;
}
static int driver_group_timer(neu_plugin_t *plugin, neu_plugin_group_t *group)
{

    if (plugin->index == 0) {
        return 0;
    }

    utarray_foreach(group->tags, neu_datatag_t *, tag)
    {
        neu_dvalue_t dvalue = { 0 };
        dvalue.type         = NEU_TYPE_STRING;
        pthread_mutex_lock(&plugin->mutex);
        strncpy(dvalue.value.str, plugin->buffer, plugin->index);
        plugin->index = 0;
        memset(plugin->buffer, 0, plugin->buf_size);
        pthread_mutex_unlock(&plugin->mutex);
        plugin->common.adapter_callbacks->driver.update(
            plugin->common.adapter, group->group_name, tag->name, dvalue);
    }
    return 0;
}
static int driver_validate_tag(neu_plugin_t *plugin, neu_datatag_t *tag)
{
    plog_debug(plugin, "validate tag success, name:%s, address:%s,type:%d ",
               tag->name, tag->address, tag->type);
    return 0;
}
static int driver_write_tag(neu_plugin_t *plugin, void *req, neu_datatag_t *tag,
                            neu_value_u value)
{
    UNUSED(plugin);
    UNUSED(req);
    UNUSED(tag);
    UNUSED(value);
    return 0;
}

static int udp_io_cb(enum neu_event_io_type type, int fd, void *usr_data)
{

    neu_plugin_t *plugin = (neu_plugin_t *) usr_data;

    if (type != NEU_EVENT_IO_READ) {
        plog_warn(plugin, "recv close, del io event");
        pthread_mutex_lock(&plugin->mutex);
        udp_server_stop(plugin);
        pthread_mutex_unlock(&plugin->mutex);
        return 0;
    }

    char buf[64] = { 0 };

    ssize_t res = recvfrom(fd, buf, sizeof(buf), MSG_DONTWAIT, NULL, NULL);
    if (-1 == res) {
        plog_warn(plugin, "recvfrom error:%s", strerror(errno));
        return 0;
    }

    if (res > 0) {
        pthread_mutex_lock(&plugin->mutex);
        if (plugin->index + res > plugin->buf_size) {
            plog_warn(plugin, "buffer is full, dump old data!");
            plugin->index = 0;
            memset(plugin->buffer, 0, plugin->buf_size);
        }

        memcpy(plugin->buffer + plugin->index, buf, res);
        plugin->index += res;
        pthread_mutex_unlock(&plugin->mutex);
    }

    return 0;
}

static int udp_server_init(neu_plugin_t *plugin)
{
    if (plugin->running) {
        plog_warn(plugin, "udp server is running");
        return -1;
    }

    int sfd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (-1 == sfd) {
        plog_error(plugin, "create socket error:%s", strerror(errno));
        return -1;
    }

    int fd_flags = fcntl(sfd, F_GETFL, 0);
    fd_flags |= O_NONBLOCK;
    fcntl(sfd, F_SETFD, fd_flags);

    struct sockaddr_in sa = { 0 };
    sa.sin_family         = AF_INET;
    sa.sin_addr.s_addr    = INADDR_ANY;
    sa.sin_port           = htons(plugin->port);

    int ret = bind(sfd, (struct sockaddr *) &sa, sizeof(struct sockaddr_in));
    if (-1 == ret) {
        plog_error(plugin, "socket bind error:%s", strerror(errno));
        return -1;
    }

    plugin->udp_server_fd = sfd;

    neu_event_io_param_t param = { 0 };
    param.fd                   = sfd;
    param.cb                   = udp_io_cb;
    param.usr_data             = plugin;

    plugin->event = neu_event_add_io(plugin->udp_server_events, param);

    plugin->running = true;

    plugin->common.link_state = NEU_NODE_LINK_STATE_CONNECTED;

    plog_info(plugin, "udp server at %d start successful!", plugin->port);

    return 0;
}

static int udp_server_stop(neu_plugin_t *plugin)
{
    if (plugin->running) {
        neu_event_del_io(plugin->udp_server_events, plugin->event);
        close(plugin->udp_server_fd);
        plugin->udp_server_fd     = -1;
        plugin->event             = NULL;
        plugin->running           = false;
        plugin->common.link_state = NEU_NODE_LINK_STATE_DISCONNECTED;
        plugin->index             = 0;
        memset(plugin->buffer, 0, plugin->buf_size);
    }
    return 0;
}
