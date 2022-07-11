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

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include <nng/nng.h>
#include <nng/protocol/pair1/pair.h>
#include <nng/supplemental/util/platform.h>

#include "utils/log.h"

#include "adapter.h"
#include "adapter_internal.h"
#include "driver/driver_internal.h"
#include "errcodes.h"
#include "persist/persist.h"
#include "plugin.h"
#include "storage.h"

static int adapter_loop(enum neu_event_io_type type, int fd, void *usr_data);
static int adapter_command(neu_adapter_t *adapter, neu_reqresp_head_t header,
                           void *data);
static int adapter_response(neu_adapter_t *adapter, neu_reqresp_head_t *header,
                            void *data);
inline static void reply(neu_adapter_t *adapter, neu_reqresp_head_t *header,
                         void *data);
static int         update_timestamp(void *usr_data);

static const adapter_callbacks_t callback_funs = {
    .command  = adapter_command,
    .response = adapter_response,
};

neu_adapter_t *neu_adapter_create(neu_adapter_info_t *info, bool start)
{
    int                     rv          = 0;
    neu_adapter_t *         adapter     = NULL;
    neu_event_io_param_t    param       = { 0 };
    neu_event_timer_param_t timer_param = {
        .second      = 0,
        .millisecond = 100,
        .cb          = update_timestamp,
    };

    switch (info->module->type) {
    case NEU_NA_TYPE_DRIVER:
        adapter = (neu_adapter_t *) neu_adapter_driver_create();
        break;
    case NEU_NA_TYPE_APP:
        adapter = calloc(1, sizeof(neu_adapter_t));
        break;
    }

    adapter->name             = strdup(info->name);
    adapter->events           = neu_event_new();
    adapter->state            = NEU_NODE_RUNNING_STATE_INIT;
    adapter->handle           = info->handle;
    adapter->cb_funs.command  = callback_funs.command;
    adapter->cb_funs.response = callback_funs.response;
    adapter->module           = info->module;
    adapter->persister        = neu_persister_create("persistence");

    rv = nng_pair1_open(&adapter->sock);
    assert(rv == 0);
    nng_socket_set_ms(adapter->sock, NNG_OPT_SENDTIMEO, 100);
    nng_socket_get_int(adapter->sock, NNG_OPT_RECVFD, &adapter->recv_fd);

    switch (info->module->type) {
    case NEU_NA_TYPE_DRIVER:
        neu_adapter_driver_init((neu_adapter_driver_t *) adapter);
        break;
    case NEU_NA_TYPE_APP:
        break;
    }

    adapter->plugin = adapter->module->intf_funs->open();
    assert(adapter->plugin != NULL);
    assert(neu_plugin_common_check(adapter->plugin));
    neu_plugin_common_t *common = neu_plugin_to_plugin_common(adapter->plugin);
    common->adapter             = adapter;
    common->adapter_callbacks   = &adapter->cb_funs;
    common->link_state          = NEU_NODE_LINK_STATE_DISCONNECTED;
    common->log                 = zlog_get_category(adapter->name);

    adapter->module->intf_funs->init(adapter->plugin);
    if (adapter_load_setting(adapter->persister, adapter->name,
                             &adapter->setting) == 0) {
        if (adapter->module->intf_funs->setting(adapter->plugin,
                                                adapter->setting) == 0) {
            adapter->state = NEU_NODE_RUNNING_STATE_READY;
        } else {
            free(adapter->setting);
            adapter->setting = NULL;
        }
        if (adapter->state == NEU_NODE_RUNNING_STATE_READY && start) {
            adapter->module->intf_funs->start(adapter->plugin);
            adapter->state = NEU_NODE_RUNNING_STATE_RUNNING;
        }
    }

    if (info->module->type == NEU_NA_TYPE_DRIVER) {
        adapter_load_group_and_tag(adapter->persister,
                                   (neu_adapter_driver_t *) adapter);
    }

    param.fd       = adapter->recv_fd;
    param.usr_data = (void *) adapter;
    param.cb       = adapter_loop;

    adapter->nng_io = neu_event_add_io(adapter->events, param);
    rv = nng_dial(adapter->sock, neu_manager_get_url(), &adapter->dialer, 0);
    assert(rv == 0);

    timer_param.usr_data = (void *) adapter;
    adapter->timer       = neu_event_add_timer(adapter->events, timer_param);

    nlog_info("Success to create adapter: %s", adapter->name);

    return adapter;
}

void neu_adapter_init(neu_adapter_t *adapter)
{
    neu_reqresp_head_t  header   = { .type = NEU_REQ_NODE_INIT };
    neu_req_node_init_t init     = { 0 };
    nng_msg *           init_msg = NULL;

    strcpy(init.node, adapter->name);
    init_msg = neu_msg_gen(&header, &init);

    nng_sendmsg(adapter->sock, init_msg, 0);
}

static int adapter_command(neu_adapter_t *adapter, neu_reqresp_head_t header,
                           void *data)
{
    int      ret = 0;
    nng_msg *msg = NULL;

    strcpy(header.sender, adapter->name);
    switch (header.type) {
    case NEU_REQ_READ_GROUP:
    case NEU_REQ_WRITE_TAG: {
        neu_req_read_group_t *cmd = (neu_req_read_group_t *) data;
        strcpy(header.receiver, cmd->driver);
        break;
    }
    case NEU_REQ_GET_GROUP:
    case NEU_REQ_DEL_GROUP:
    case NEU_REQ_ADD_GROUP: {
        neu_req_add_group_t *cmd = (neu_req_add_group_t *) data;
        strcpy(header.receiver, cmd->driver);
        break;
    }
    case NEU_REQ_GET_TAG:
    case NEU_REQ_UPDATE_TAG:
    case NEU_REQ_DEL_TAG:
    case NEU_REQ_ADD_TAG: {
        neu_req_add_tag_t *cmd = (neu_req_add_tag_t *) data;
        strcpy(header.receiver, cmd->driver);
        break;
    }
    case NEU_REQ_NODE_CTL:
    case NEU_REQ_GET_NODE_STATE:
    case NEU_REQ_GET_NODE_SETTING:
    case NEU_REQ_NODE_SETTING: {
        neu_req_node_setting_t *cmd = (neu_req_node_setting_t *) data;
        strcpy(header.receiver, cmd->node);
        break;
    }
    default:
        break;
    }

    msg = neu_msg_gen(&header, data);

    ret = nng_sendmsg(adapter->sock, msg, 0);
    if (ret != 0) {
        nng_msg_free(msg);
    }

    return ret;
}

static int adapter_response(neu_adapter_t *adapter, neu_reqresp_head_t *header,
                            void *data)
{
    switch (header->type) {
    case NEU_REQRESP_TRANS_DATA:
        strcpy(header->sender, adapter->name);
        break;
    default:
        neu_msg_exchange(header);
        break;
    }

    nng_msg *msg = neu_msg_gen(header, data);
    int      ret = nng_sendmsg(adapter->sock, msg, 0);
    if (ret != 0) {
        nng_msg_free(msg);
    }

    return ret;
}

static int adapter_loop(enum neu_event_io_type type, int fd, void *usr_data)
{
    neu_adapter_t *     adapter = (neu_adapter_t *) usr_data;
    nng_msg *           msg     = NULL;
    neu_reqresp_head_t *header  = NULL;

    if (type != NEU_EVENT_IO_READ) {
        nlog_warn("adapter: %s recv close, exit loop, fd: %d", adapter->name,
                  fd);
        return 0;
    }

    nng_recvmsg(adapter->sock, &msg, 0);
    header = (neu_reqresp_head_t *) nng_msg_body(msg);

    switch (header->type) {
    case NEU_REQ_UPDATE_LICENSE:
    case NEU_REQ_APP_UNSUBSCRIBE_GROUP:
    case NEU_RESP_GET_NODE_STATE:
    case NEU_RESP_GET_NODE_SETTING:
    case NEU_RESP_READ_GROUP:
    case NEU_RESP_GET_SUBSCRIBE_GROUP:
    case NEU_RESP_ADD_TAG:
    case NEU_RESP_UPDATE_TAG:
    case NEU_RESP_GET_TAG:
    case NEU_RESP_GET_NODE:
    case NEU_RESP_GET_PLUGIN:
    case NEU_RESP_GET_GROUP:
    case NEU_RESP_ERROR:
    case NEU_REQRESP_TRANS_DATA:
    case NEU_RESP_APP_SUBSCRIBE_GROUP:
        adapter->module->intf_funs->request(
            adapter->plugin, (neu_reqresp_head_t *) header, &header[1]);
        break;
    case NEU_REQ_READ_GROUP: {
        neu_resp_error_t error = { 0 };

        if (adapter->module->type != NEU_NA_TYPE_DRIVER) {
            error.error  = NEU_ERR_GROUP_NOT_ALLOW;
            header->type = NEU_RESP_ERROR;
            neu_msg_exchange(header);
            reply(adapter, header, &error);
        } else {
            neu_adapter_driver_read_group((neu_adapter_driver_t *) adapter,
                                          header);
        }

        break;
    }
    case NEU_REQ_WRITE_TAG: {
        neu_resp_error_t error = { 0 };

        if (adapter->module->type != NEU_NA_TYPE_DRIVER) {
            error.error  = NEU_ERR_GROUP_NOT_ALLOW;
            header->type = NEU_RESP_ERROR;
            neu_msg_exchange(header);
            reply(adapter, header, &error);
        } else {
            void *msg_dump = calloc(nng_msg_len(msg), 1);
            memcpy(msg_dump, header, nng_msg_len(msg));
            neu_adapter_driver_write_tag((neu_adapter_driver_t *) adapter,
                                         msg_dump);
        }

        break;
    }
    case NEU_REQ_NODE_SETTING: {
        neu_req_node_setting_t *cmd   = (neu_req_node_setting_t *) &header[1];
        neu_resp_error_t        error = { 0 };

        error.error = neu_adapter_set_setting(adapter, cmd->setting);
        if (error.error == NEU_ERR_SUCCESS) {
            adapter_storage_setting(adapter->persister, adapter->name,
                                    cmd->setting);
        }
        free(cmd->setting);

        header->type = NEU_RESP_ERROR;
        neu_msg_exchange(header);
        reply(adapter, header, &error);
        break;
    }
    case NEU_REQ_GET_NODE_SETTING: {
        neu_resp_get_node_setting_t resp  = { 0 };
        neu_resp_error_t            error = { 0 };

        neu_msg_exchange(header);
        error.error = neu_adapter_get_setting(adapter, &resp.setting);
        if (error.error != NEU_ERR_SUCCESS) {
            header->type = NEU_RESP_ERROR;
            reply(adapter, header, &error);
        } else {
            header->type = NEU_RESP_GET_NODE_SETTING;
            reply(adapter, header, &resp);
        }

        break;
    }
    case NEU_REQ_GET_NODE_STATE: {
        neu_resp_get_node_state_t resp = { 0 };

        resp.state   = neu_adapter_get_state(adapter);
        header->type = NEU_RESP_GET_NODE_STATE;
        neu_msg_exchange(header);
        reply(adapter, header, &resp);
        break;
    }
    case NEU_REQ_GET_GROUP: {
        neu_msg_exchange(header);

        if (adapter->module->type != NEU_NA_TYPE_DRIVER) {
            neu_resp_error_t error = { .error = NEU_ERR_GROUP_NOT_ALLOW };

            header->type = NEU_RESP_ERROR;
            reply(adapter, header, &error);
        } else {
            neu_resp_get_group_t resp = {
                .groups = neu_adapter_driver_get_group(
                    (neu_adapter_driver_t *) adapter)
            };
            header->type = NEU_RESP_GET_GROUP;
            reply(adapter, header, &resp);
        }
        break;
    }
    case NEU_REQ_APP_SUBSCRIBE_GROUP: {
        neu_req_app_subscribe_group_t *cmd =
            (neu_req_app_subscribe_group_t *) &header[1];
        neu_resp_app_subscribe_group_t resp = { 0 };

        neu_adapter_driver_get_tag((neu_adapter_driver_t *) adapter, cmd->group,
                                   &resp.tags);
        neu_msg_exchange(header);
        strcpy(resp.driver, cmd->driver);
        strcpy(resp.group, cmd->group);

        header->type = NEU_RESP_APP_SUBSCRIBE_GROUP;
        reply(adapter, header, &resp);
        break;
    }
    case NEU_REQ_GET_TAG: {
        neu_req_get_tag_t *cmd   = (neu_req_get_tag_t *) &header[1];
        neu_resp_error_t   error = { .error = 0 };
        UT_array *         tags  = NULL;

        if (adapter->module->type != NEU_NA_TYPE_DRIVER) {
            error.error = NEU_ERR_GROUP_NOT_ALLOW;
        } else {
            error.error = neu_adapter_driver_get_tag(
                (neu_adapter_driver_t *) adapter, cmd->group, &tags);
        }

        neu_msg_exchange(header);
        if (error.error != NEU_ERR_SUCCESS) {
            header->type = NEU_RESP_ERROR;
            reply(adapter, header, &error);
        } else {
            neu_resp_get_tag_t resp = { .tags = tags };

            header->type = NEU_RESP_GET_TAG;
            reply(adapter, header, &resp);
        }

        break;
    }
    case NEU_REQ_ADD_GROUP: {
        neu_req_add_group_t *cmd   = (neu_req_add_group_t *) &header[1];
        neu_resp_error_t     error = { 0 };

        if (cmd->interval < NEU_GROUP_INTERVAL_LIMIT) {
            error.error = NEU_ERR_GROUP_PARAMETER_INVALID;
        } else {
            if (adapter->module->type != NEU_NA_TYPE_DRIVER) {
                error.error = NEU_ERR_GROUP_NOT_ALLOW;
            } else {
                error.error = neu_adapter_driver_add_group(
                    (neu_adapter_driver_t *) adapter, cmd->group,
                    cmd->interval);
            }
        }

        if (error.error == NEU_ERR_SUCCESS) {
            adapter_storage_add_group(adapter->persister, adapter->name,
                                      cmd->group, cmd->interval);
        }

        neu_msg_exchange(header);
        header->type = NEU_RESP_ERROR;
        reply(adapter, header, &error);
        break;
    }
    case NEU_REQ_DEL_GROUP: {
        neu_req_del_group_t *cmd   = (neu_req_del_group_t *) &header[1];
        neu_resp_error_t     error = { 0 };

        if (adapter->module->type != NEU_NA_TYPE_DRIVER) {
            error.error = NEU_ERR_GROUP_NOT_ALLOW;
        } else {
            error.error = neu_adapter_driver_del_group(
                (neu_adapter_driver_t *) adapter, cmd->group);
        }

        if (error.error == NEU_ERR_SUCCESS) {
            adapter_storage_del_group(adapter->persister, cmd->driver,
                                      cmd->group);
        }

        neu_msg_exchange(header);
        header->type = NEU_RESP_ERROR;
        reply(adapter, header, &error);
        break;
    }
    case NEU_REQ_NODE_CTL: {
        neu_req_node_ctl_t *cmd   = (neu_req_node_ctl_t *) &header[1];
        neu_resp_error_t    error = { 0 };

        switch (cmd->ctl) {
        case NEU_ADAPTER_CTL_START:
            error.error = neu_adapter_start(adapter);
            break;
        case NEU_ADAPTER_CTL_STOP:
            error.error = neu_adapter_stop(adapter);
            break;
        }

        neu_msg_exchange(header);
        header->type = NEU_RESP_ERROR;
        reply(adapter, header, &error);
        break;
    }
    case NEU_REQ_DEL_TAG: {
        neu_req_del_tag_t *cmd   = (neu_req_del_tag_t *) &header[1];
        neu_resp_error_t   error = { 0 };

        if (adapter->module->type != NEU_NA_TYPE_DRIVER) {
            error.error = NEU_ERR_GROUP_NOT_ALLOW;
        } else {
            for (int i = 0; i < cmd->n_tag; i++) {
                neu_adapter_driver_del_tag((neu_adapter_driver_t *) adapter,
                                           cmd->group, cmd->tags[i]);
            }
        }

        for (int i = 0; i < cmd->n_tag; i++) {
            free(cmd->tags[i]);
        }
        free(cmd->tags);

        if (error.error == NEU_ERR_SUCCESS) {
            adapter_storage_tag(adapter->persister, adapter, cmd->driver,
                                cmd->group);
        }

        neu_msg_exchange(header);
        header->type = NEU_RESP_ERROR;
        reply(adapter, header, &error);
        break;
    }
    case NEU_REQ_ADD_TAG: {
        neu_req_add_tag_t *cmd  = (neu_req_add_tag_t *) &header[1];
        neu_resp_add_tag_t resp = { 0 };

        if (adapter->module->type != NEU_NA_TYPE_DRIVER) {
            resp.error = NEU_ERR_GROUP_NOT_ALLOW;
        } else {
            for (int i = 0; i < cmd->n_tag; i++) {
                int ret =
                    neu_adapter_driver_add_tag((neu_adapter_driver_t *) adapter,
                                               cmd->group, &cmd->tags[i]);
                if (ret == 0) {
                    resp.index += 1;
                } else {
                    resp.error = ret;
                    break;
                }
            }
        }
        for (int i = 0; i < cmd->n_tag; i++) {
            free(cmd->tags[i].address);
            free(cmd->tags[i].name);
            free(cmd->tags[i].description);
        }
        free(cmd->tags);

        if (resp.index > 0) {
            adapter_storage_tag(adapter->persister, adapter, cmd->driver,
                                cmd->group);
        }

        neu_msg_exchange(header);
        header->type = NEU_RESP_ADD_TAG;
        reply(adapter, header, &resp);
        break;
    }
    case NEU_REQ_UPDATE_TAG: {
        neu_req_update_tag_t *cmd  = (neu_req_update_tag_t *) &header[1];
        neu_resp_update_tag_t resp = { 0 };

        if (adapter->module->type != NEU_NA_TYPE_DRIVER) {
            resp.error = NEU_ERR_GROUP_NOT_ALLOW;
        } else {
            for (int i = 0; i < cmd->n_tag; i++) {
                int ret = neu_adapter_driver_update_tag(
                    (neu_adapter_driver_t *) adapter, cmd->group,
                    &cmd->tags[i]);
                if (ret == 0) {
                    resp.index += 1;
                } else {
                    resp.error = ret;
                    break;
                }
            }
        }
        for (int i = 0; i < cmd->n_tag; i++) {
            free(cmd->tags[i].address);
            free(cmd->tags[i].name);
            free(cmd->tags[i].description);
        }
        free(cmd->tags);

        if (resp.index > 0) {
            adapter_storage_tag(adapter->persister, adapter, cmd->driver,
                                cmd->group);
        }

        neu_msg_exchange(header);
        header->type = NEU_RESP_UPDATE_TAG;
        reply(adapter, header, &resp);
        break;
    }
    case NEU_REQ_NODE_UNINIT: {
        neu_req_node_uninit_t *cmd = (neu_req_node_uninit_t *) &header[1];
        nng_msg *              uninit_msg                  = NULL;
        char                   name[NEU_NODE_NAME_LEN]     = { 0 };
        char                   receiver[NEU_NODE_NAME_LEN] = { 0 };

        neu_adapter_uninit(adapter);

        header->type = NEU_RESP_NODE_UNINIT;
        neu_msg_exchange(header);
        strcpy(header->sender, adapter->name);
        strcpy(cmd->node, adapter->name);
        uninit_msg = neu_msg_gen(header, cmd);

        strcpy(name, adapter->name);
        strcpy(receiver, header->receiver);
        if (nng_sendmsg(adapter->sock, uninit_msg, 0) == 0) {
            nlog_warn("%s send uninit msg to %s", name, receiver);
        } else {
            nng_msg_free(uninit_msg);
        }
        break;
    }

    default:
        assert(false);
        break;
    }
    nng_msg_free(msg);

    return 0;
}

void neu_adapter_destroy(neu_adapter_t *adapter)
{
    nng_dialer_close(adapter->dialer);
    nng_close(adapter->sock);
    neu_persister_destroy(adapter->persister);

    if (adapter->name != NULL) {
        free(adapter->name);
    }
    if (NULL != adapter->setting) {
        free(adapter->setting);
    }

    neu_event_close(adapter->events);
    free(adapter);
}

int neu_adapter_uninit(neu_adapter_t *adapter)
{
    if (adapter->module->type == NEU_NA_TYPE_DRIVER) {
        neu_adapter_driver_uninit((neu_adapter_driver_t *) adapter);
    }
    adapter->module->intf_funs->uninit(adapter->plugin);

    neu_event_del_io(adapter->events, adapter->nng_io);

    if (adapter->module->type == NEU_NA_TYPE_DRIVER) {
        neu_adapter_driver_destroy((neu_adapter_driver_t *) adapter);
    }

    neu_event_del_timer(adapter->events, adapter->timer);
    adapter->module->intf_funs->close(adapter->plugin);

    nlog_info("Stop the adapter(%s)", adapter->name);
    return 0;
}

int neu_adapter_start(neu_adapter_t *adapter)
{
    const neu_plugin_intf_funs_t *intf_funs = adapter->module->intf_funs;
    neu_err_code_e                error     = NEU_ERR_SUCCESS;

    switch (adapter->state) {
    case NEU_NODE_RUNNING_STATE_IDLE:
    case NEU_NODE_RUNNING_STATE_INIT:
        if (strcmp(adapter->module->module_name, "ekuiper") != 0) {
            error = NEU_ERR_NODE_NOT_READY;
        }
        break;
    case NEU_NODE_RUNNING_STATE_RUNNING:
        error = NEU_ERR_NODE_IS_RUNNING;
        break;
    case NEU_NODE_RUNNING_STATE_READY:
    case NEU_NODE_RUNNING_STATE_STOPPED:
        break;
    }

    if (error != NEU_ERR_SUCCESS) {
        return error;
    }

    error = intf_funs->start(adapter->plugin);
    if (error == NEU_ERR_SUCCESS) {
        adapter->state = NEU_NODE_RUNNING_STATE_RUNNING;
    }

    return error;
}

int neu_adapter_stop(neu_adapter_t *adapter)
{
    const neu_plugin_intf_funs_t *intf_funs = adapter->module->intf_funs;
    neu_err_code_e                error     = NEU_ERR_SUCCESS;

    switch (adapter->state) {
    case NEU_NODE_RUNNING_STATE_IDLE:
    case NEU_NODE_RUNNING_STATE_INIT:
    case NEU_NODE_RUNNING_STATE_READY:
        error = NEU_ERR_NODE_NOT_RUNNING;
        break;
    case NEU_NODE_RUNNING_STATE_STOPPED:
        error = NEU_ERR_NODE_IS_STOPED;
        break;
    case NEU_NODE_RUNNING_STATE_RUNNING:
        break;
    }

    if (error != NEU_ERR_SUCCESS) {
        return error;
    }

    error = intf_funs->stop(adapter->plugin);
    if (error == NEU_ERR_SUCCESS) {
        adapter->state = NEU_NODE_RUNNING_STATE_STOPPED;
    }

    return error;
}

int neu_adapter_set_setting(neu_adapter_t *adapter, const char *setting)
{
    int rv = -1;

    const neu_plugin_intf_funs_t *intf_funs;

    intf_funs = adapter->module->intf_funs;
    rv        = intf_funs->setting(adapter->plugin, setting);
    if (rv == 0) {
        if (adapter->setting != NULL) {
            free(adapter->setting);
        }
        adapter->setting = strdup(setting);

        if (adapter->state == NEU_NODE_RUNNING_STATE_INIT) {
            adapter->state = NEU_NODE_RUNNING_STATE_READY;
        }
    } else {
        rv = NEU_ERR_NODE_SETTING_INVALID;
    }

    return rv;
}

int neu_adapter_get_setting(neu_adapter_t *adapter, char **config)
{
    if (adapter->setting != NULL) {
        *config = strdup(adapter->setting);
        return NEU_ERR_SUCCESS;
    }

    return NEU_ERR_NODE_SETTING_NOT_FOUND;
}

neu_node_state_t neu_adapter_get_state(neu_adapter_t *adapter)
{
    neu_node_state_t     state  = { 0 };
    neu_plugin_common_t *common = neu_plugin_to_plugin_common(adapter->plugin);

    state.link    = common->link_state;
    state.running = adapter->state;

    return state;
}

int neu_adapter_validate_tag(neu_adapter_t *adapter, neu_datatag_t *tag)
{
    const neu_plugin_intf_funs_t *intf_funs = adapter->module->intf_funs;
    neu_err_code_e                error     = NEU_ERR_SUCCESS;

    error = intf_funs->driver.validate_tag(adapter->plugin, tag);

    return error;
}

neu_event_timer_t *neu_adapter_add_timer(neu_adapter_t *         adapter,
                                         neu_event_timer_param_t param)
{
    return neu_event_add_timer(adapter->events, param);
}

void neu_adapter_del_timer(neu_adapter_t *adapter, neu_event_timer_t *timer)
{
    neu_event_del_timer(adapter->events, timer);
}

inline static void reply(neu_adapter_t *adapter, neu_reqresp_head_t *header,
                         void *data)
{
    nng_msg *msg = neu_msg_gen(header, data);

    nng_sendmsg(adapter->sock, msg, 0);
}

static int update_timestamp(void *usr_data)
{
    neu_adapter_t *adapter   = (neu_adapter_t *) usr_data;
    struct timeval tv        = { 0 };
    uint64_t       timestamp = 0;

    gettimeofday(&tv, NULL);
    timestamp          = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    adapter->timestamp = timestamp;

    neu_plugin_to_plugin_common(adapter->plugin)->timestamp = timestamp;
    return 0;
}