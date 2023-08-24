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
#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
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
static int adapter_register_metric(neu_adapter_t *adapter, const char *name,
                                   const char *help, neu_metric_type_e type,
                                   uint64_t init);
static int adapter_update_metric(neu_adapter_t *adapter,
                                 const char *metric_name, uint64_t n,
                                 const char *group);
inline static void reply(neu_adapter_t *adapter, neu_reqresp_head_t *header,
                         void *data);
inline static void notify_monitor(neu_adapter_t *    adapter,
                                  neu_reqresp_type_e event, void *data);
static int         level_check(void *usr_data);

static const adapter_callbacks_t callback_funs = {
    .command         = adapter_command,
    .response        = adapter_response,
    .register_metric = adapter_register_metric,
    .update_metric   = adapter_update_metric,
};

static __thread int create_adapter_error = 0;

#define REGISTER_METRIC(adapter, name, init) \
    adapter_register_metric(adapter, name, name##_HELP, name##_TYPE, init);

#define REGISTER_DRIVER_METRICS(adapter)                     \
    REGISTER_METRIC(adapter, NEU_METRIC_LINK_STATE,          \
                    NEU_NODE_LINK_STATE_DISCONNECTED);       \
    REGISTER_METRIC(adapter, NEU_METRIC_RUNNING_STATE,       \
                    NEU_NODE_RUNNING_STATE_INIT);            \
    REGISTER_METRIC(adapter, NEU_METRIC_LAST_RTT_MS,         \
                    NEU_METRIC_LAST_RTT_MS_MAX);             \
    REGISTER_METRIC(adapter, NEU_METRIC_SEND_BYTES, 0);      \
    REGISTER_METRIC(adapter, NEU_METRIC_RECV_BYTES, 0);      \
    REGISTER_METRIC(adapter, NEU_METRIC_TAG_READS_TOTAL, 0); \
    REGISTER_METRIC(adapter, NEU_METRIC_TAG_READ_ERRORS_TOTAL, 0);

#define REGISTER_APP_METRICS(adapter)                              \
    REGISTER_METRIC(adapter, NEU_METRIC_LINK_STATE,                \
                    NEU_NODE_LINK_STATE_DISCONNECTED);             \
    REGISTER_METRIC(adapter, NEU_METRIC_RUNNING_STATE,             \
                    NEU_NODE_RUNNING_STATE_INIT);                  \
    REGISTER_METRIC(adapter, NEU_METRIC_SEND_MSGS_TOTAL, 0);       \
    REGISTER_METRIC(adapter, NEU_METRIC_SEND_MSG_ERRORS_TOTAL, 0); \
    REGISTER_METRIC(adapter, NEU_METRIC_RECV_MSGS_TOTAL, 0);

static inline void start_log_level_timer(neu_adapter_t *adapter)
{
    neu_event_timer_param_t timer_level = {
        .second      = 30,
        .millisecond = 0,
        .cb          = level_check,
        .usr_data    = adapter,
        .type        = NEU_EVENT_TIMER_BLOCK,
    };

    adapter->timer_lev = neu_event_add_timer(adapter->events, timer_level);
}

static inline void stop_log_level_timer(neu_adapter_t *adapter)
{
    neu_event_del_timer(adapter->events, adapter->timer_lev);
    adapter->timer_lev = NULL;
}

static UT_array *collect_logs(const char *dir, const char *node)
{
    DIR *          dirp                         = NULL;
    struct dirent *dent                         = NULL;
    UT_array *     files                        = NULL;
    char           flag[NEU_NODE_NAME_LEN + 2]  = { 0 };
    char           flag_[NEU_NODE_NAME_LEN + 2] = { 0 };

    if ((dirp = opendir(dir)) == NULL) {
        nlog_error("fail open dir: %s", dir);
        return NULL;
    }

    utarray_new(files, &ut_str_icd);
    snprintf(flag, sizeof(flag), "%s.", node);
    snprintf(flag_, sizeof(flag_), "%s_", node);

    while (NULL != (dent = readdir(dirp))) {
        if (strstr(dent->d_name, flag) != NULL ||
            strstr(dent->d_name, flag_) != NULL) {
            char *file = dent->d_name;
            utarray_push_back(files, &file);
        }
    }

    closedir(dirp);
    return files;
}

int neu_adapter_error()
{
    return create_adapter_error;
}

void neu_adapter_set_error(int error)
{
    create_adapter_error = error;
}

neu_adapter_t *neu_adapter_create(neu_adapter_info_t *info, bool load)
{
    int                  rv      = 0;
    int                  init_rv = 0;
    neu_adapter_t *      adapter = NULL;
    neu_event_io_param_t param   = { 0 };

    switch (info->module->type) {
    case NEU_NA_TYPE_DRIVER:
        adapter = (neu_adapter_t *) neu_adapter_driver_create();
        break;
    case NEU_NA_TYPE_NDRIVER:
    case NEU_NA_TYPE_APP:
        adapter = calloc(1, sizeof(neu_adapter_t));
        break;
    }

    adapter->name                    = strdup(info->name);
    adapter->events                  = neu_event_new();
    adapter->state                   = NEU_NODE_RUNNING_STATE_INIT;
    adapter->handle                  = info->handle;
    adapter->cb_funs.command         = callback_funs.command;
    adapter->cb_funs.response        = callback_funs.response;
    adapter->cb_funs.register_metric = callback_funs.register_metric;
    adapter->cb_funs.update_metric   = callback_funs.update_metric;
    adapter->module                  = info->module;

    adapter->timestamp_lev = 0;

    rv = nng_pair1_open(&adapter->sock);
    assert(rv == 0);
    nng_socket_set_int(adapter->sock, NNG_OPT_RECVBUF, 8192);
    nng_socket_set_int(adapter->sock, NNG_OPT_SENDBUF, 8192);
    nng_socket_set_ms(adapter->sock, NNG_OPT_SENDTIMEO, 1000);
    nng_socket_get_int(adapter->sock, NNG_OPT_RECVFD, &adapter->recv_fd);

    switch (info->module->type) {
    case NEU_NA_TYPE_DRIVER:
        if (adapter->module->display) {
            REGISTER_DRIVER_METRICS(adapter);
        }
        neu_adapter_driver_init((neu_adapter_driver_t *) adapter);
        break;
    case NEU_NA_TYPE_NDRIVER:
    case NEU_NA_TYPE_APP:
        if (adapter->module->display) {
            REGISTER_APP_METRICS(adapter);
        }
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
    strcpy(common->name, adapter->name);

    zlog_level_switch(common->log, default_log_level);

    init_rv = adapter->module->intf_funs->init(adapter->plugin, load);

    if (adapter_load_setting(adapter->name, &adapter->setting) == 0) {
        if (adapter->module->intf_funs->setting(adapter->plugin,
                                                adapter->setting) == 0) {
            adapter->state = NEU_NODE_RUNNING_STATE_READY;
        } else {
            free(adapter->setting);
            adapter->setting = NULL;
        }
    }

    if (info->module->type == NEU_NA_TYPE_DRIVER) {
        adapter_load_group_and_tag((neu_adapter_driver_t *) adapter);
    }

    param.fd       = adapter->recv_fd;
    param.usr_data = (void *) adapter;
    param.cb       = adapter_loop;

    adapter->nng_io = neu_event_add_io(adapter->events, param);
    rv = nng_dial(adapter->sock, neu_manager_get_url(), &adapter->dialer, 0);
    assert(rv == 0);

    start_log_level_timer(adapter);

    nlog_notice("Success to create adapter: %s", adapter->name);

    adapter_storage_state(adapter->name, adapter->state);

    if (init_rv != 0) {
        nlog_warn("Failed to init adapter: %s", adapter->name);
        neu_adapter_set_error(init_rv);

        if (adapter->module->type == NEU_NA_TYPE_DRIVER) {
            neu_adapter_driver_destroy((neu_adapter_driver_t *) adapter);
        }
        stop_log_level_timer(adapter);
        neu_event_del_io(adapter->events, adapter->nng_io);

        neu_adapter_destroy(adapter);
        return NULL;
    } else {
        neu_adapter_set_error(0);
        return adapter;
    }
}

int neu_adapter_rename(neu_adapter_t *adapter, const char *new_name)
{
    char *name = strdup(new_name);
    if (NULL == name) {
        return NEU_ERR_EINTERNAL;
    }

    zlog_category_t *log = zlog_get_category(name);
    if (NULL == log) {
        free(name);
        return NEU_ERR_EINTERNAL;
    }

    stop_log_level_timer(adapter);
    if (NEU_NA_TYPE_DRIVER == adapter->module->type) {
        neu_adapter_driver_stop_group_timer((neu_adapter_driver_t *) adapter);
    }

    // fix metrics
    if (adapter->metrics) {
        neu_metrics_del_node(adapter);
    }
    free(adapter->name);
    adapter->name = name;
    if (adapter->metrics) {
        adapter->metrics->name = name;
        neu_metrics_add_node(adapter);
    }

    // fix log
    neu_plugin_common_t *common = neu_plugin_to_plugin_common(adapter->plugin);
    common->log                 = log;
    strcpy(common->name, adapter->name);
    zlog_level_switch(common->log, default_log_level);

    start_log_level_timer(adapter);
    if (NEU_NA_TYPE_DRIVER == adapter->module->type) {
        neu_adapter_driver_start_group_timer((neu_adapter_driver_t *) adapter);
    }

    return 0;
}

void neu_adapter_init(neu_adapter_t *adapter, neu_node_running_state_e state)
{
    neu_reqresp_head_t  header   = { .type = NEU_REQ_NODE_INIT };
    neu_req_node_init_t init     = { 0 };
    nng_msg *           init_msg = NULL;

    strcpy(header.sender, adapter->name);
    strcpy(header.receiver, "manager");

    strcpy(init.node, adapter->name);
    init.state = state;
    init_msg   = neu_msg_gen(&header, &init);

    nng_sendmsg(adapter->sock, init_msg, 0);
}

neu_node_type_e neu_adapter_get_type(neu_adapter_t *adapter)
{
    return adapter->module->type;
}

static int adapter_register_metric(neu_adapter_t *adapter, const char *name,
                                   const char *help, neu_metric_type_e type,
                                   uint64_t init)
{
    if (NULL == adapter->metrics) {
        adapter->metrics = calloc(1, sizeof(*adapter->metrics));
        if (NULL == adapter->metrics) {
            return -1;
        }
        adapter->metrics->type    = adapter->module->type;
        adapter->metrics->name    = adapter->name;
        adapter->metrics->adapter = adapter;
        neu_metrics_add_node(adapter);
    }

    if (0 > neu_metric_entries_add(&adapter->metrics->entries, name, help, type,
                                   init)) {
        return -1;
    }

    neu_metrics_register_entry(name, help, type);
    return 0;
}

static int adapter_update_metric(neu_adapter_t *adapter,
                                 const char *metric_name, uint64_t n,
                                 const char *group)
{
    neu_metric_entry_t *entry = NULL;
    if (NULL == adapter->metrics) {
        return -1;
    }

    if (NULL == group) {
        HASH_FIND_STR(adapter->metrics->entries, metric_name, entry);
    } else if (NULL != adapter->metrics->group_metrics) {
        neu_group_metrics_t *g = NULL;
        HASH_FIND_STR(adapter->metrics->group_metrics, group, g);
        if (NULL != g) {
            HASH_FIND_STR(g->entries, metric_name, entry);
        }
    }

    if (NULL == entry) {
        return -1;
    }

    if (NEU_METRIC_TYPE_COUNTER == entry->type) {
        entry->value += n;
    } else {
        entry->value = n;
    }

    return 0;
}

static void adapter_reset_metrics(neu_adapter_t *adapter)
{
    neu_metric_entry_t *entry = NULL;
    if (NULL == adapter->metrics) {
        return;
    }

    HASH_LOOP(hh, adapter->metrics->entries, entry) { entry->value = 0; }

    neu_group_metrics_t *g = NULL;
    HASH_LOOP(hh, adapter->metrics->group_metrics, g)
    {
        HASH_LOOP(hh, g->entries, entry) { entry->value = 0; }
    }
}

static int adapter_command(neu_adapter_t *adapter, neu_reqresp_head_t header,
                           void *data)
{
    int      ret = 0;
    nng_msg *msg = NULL;

    strcpy(header.sender, adapter->name);
    switch (header.type) {
    case NEU_REQ_READ_GROUP:
    case NEU_REQ_WRITE_TAGS:
    case NEU_REQ_WRITE_TAG: {
        neu_req_read_group_t *cmd = (neu_req_read_group_t *) data;
        strcpy(header.receiver, cmd->driver);
        break;
    }
    case NEU_REQ_DEL_NODE: {
        neu_req_del_node_t *cmd = (neu_req_del_node_t *) data;
        strcpy(header.receiver, cmd->node);
        break;
    }
    case NEU_REQ_UPDATE_GROUP:
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
    case NEU_REQ_UPDATE_NODE:
    case NEU_REQ_NODE_CTL:
    case NEU_REQ_GET_NODE_STATE:
    case NEU_REQ_GET_NODE_SETTING:
    case NEU_REQ_NODE_SETTING: {
        neu_req_node_setting_t *cmd = (neu_req_node_setting_t *) data;
        strcpy(header.receiver, cmd->node);
        break;
    }
    case NEU_REQ_GET_DRIVER_GROUP:
    case NEU_REQ_GET_SUB_DRIVER_TAGS: {
        strcpy(header.receiver, "manager");
        break;
    }
    case NEU_REQRESP_NODE_DELETED: {
        neu_reqresp_node_deleted_t *cmd = (neu_reqresp_node_deleted_t *) data;
        strcpy(header.receiver, cmd->node);
        break;
    }
    case NEU_REQ_UPDATE_NDRIVER_TAG_PARAM:
    case NEU_REQ_UPDATE_NDRIVER_TAG_INFO:
    case NEU_REQ_GET_NDRIVER_TAGS: {
        neu_req_get_ndriver_tags_t *cmd = (neu_req_get_ndriver_tags_t *) data;
        strcpy(header.receiver, cmd->ndriver);
        break;
    }
    case NEU_REQ_UPDATE_LOG_LEVEL: {
        neu_req_update_log_level_t *cmd = (neu_req_update_log_level_t *) data;
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
    nlog_info("adapter(%s) recv msg from: %s, type: %s", adapter->name,
              header->sender, neu_reqresp_type_string(header->type));

    switch (header->type) {
    case NEU_RESP_GET_DRIVER_GROUP:
    case NEU_REQRESP_NODE_DELETED:
    case NEU_RESP_GET_SUB_DRIVER_TAGS:
    case NEU_REQ_UPDATE_LICENSE:
    case NEU_RESP_GET_NODE_STATE:
    case NEU_RESP_GET_NODES_STATE:
    case NEU_RESP_GET_NODE_SETTING:
    case NEU_REQ_SUBSCRIBE_GROUP:
    case NEU_REQ_UPDATE_SUBSCRIBE_GROUP:
    case NEU_REQ_UNSUBSCRIBE_GROUP:
    case NEU_RESP_READ_GROUP:
    case NEU_RESP_GET_SUBSCRIBE_GROUP:
    case NEU_RESP_ADD_TAG:
    case NEU_RESP_ADD_TEMPLATE_TAG:
    case NEU_RESP_UPDATE_TAG:
    case NEU_RESP_UPDATE_TEMPLATE_TAG:
    case NEU_RESP_GET_TAG:
    case NEU_RESP_GET_TEMPLATE_TAG:
    case NEU_RESP_GET_NODE:
    case NEU_RESP_GET_PLUGIN:
    case NEU_RESP_GET_TEMPLATE:
    case NEU_RESP_GET_TEMPLATES:
    case NEU_RESP_GET_NDRIVER_MAPS:
    case NEU_RESP_GET_NDRIVER_TAGS:
    case NEU_RESP_GET_GROUP:
    case NEU_RESP_ERROR:
    case NEU_REQRESP_TRANS_DATA:
    case NEU_REQRESP_NODES_STATE:
    case NEU_REQ_ADD_NODE_EVENT:
    case NEU_REQ_DEL_NODE_EVENT:
    case NEU_REQ_NODE_CTL_EVENT:
    case NEU_REQ_NODE_SETTING_EVENT:
    case NEU_REQ_ADD_GROUP_EVENT:
    case NEU_REQ_DEL_GROUP_EVENT:
    case NEU_REQ_UPDATE_GROUP_EVENT:
    case NEU_REQ_ADD_TAG_EVENT:
    case NEU_REQ_DEL_TAG_EVENT:
    case NEU_REQ_UPDATE_TAG_EVENT:
        adapter->module->intf_funs->request(
            adapter->plugin, (neu_reqresp_head_t *) header, &header[1]);
        break;
    case NEU_REQ_READ_GROUP: {
        neu_resp_error_t error = { 0 };

        if (adapter->module->type == NEU_NA_TYPE_DRIVER) {
            neu_adapter_driver_read_group((neu_adapter_driver_t *) adapter,
                                          header);
        } else {
            error.error  = NEU_ERR_GROUP_NOT_ALLOW;
            header->type = NEU_RESP_ERROR;
            neu_msg_exchange(header);
            reply(adapter, header, &error);
        }

        break;
    }
    case NEU_REQ_WRITE_TAG: {
        neu_resp_error_t error = { 0 };

        if (adapter->module->type == NEU_NA_TYPE_DRIVER) {
            void *msg_dump = calloc(nng_msg_len(msg), 1);
            memcpy(msg_dump, header, nng_msg_len(msg));
            neu_adapter_driver_write_tag((neu_adapter_driver_t *) adapter,
                                         msg_dump);
        } else {
            error.error  = NEU_ERR_GROUP_NOT_ALLOW;
            header->type = NEU_RESP_ERROR;
            neu_msg_exchange(header);
            reply(adapter, header, &error);
        }

        break;
    }
    case NEU_REQ_WRITE_TAGS: {
        neu_resp_error_t error = { 0 };

        if (adapter->module->type != NEU_NA_TYPE_DRIVER) {
            error.error  = NEU_ERR_GROUP_NOT_ALLOW;
            header->type = NEU_RESP_ERROR;
            neu_msg_exchange(header);
            reply(adapter, header, &error);
        } else {
            void *msg_dump = calloc(nng_msg_len(msg), 1);
            memcpy(msg_dump, header, nng_msg_len(msg));
            neu_adapter_driver_write_tags((neu_adapter_driver_t *) adapter,
                                          msg_dump);
        }
        break;
    }
    case NEU_REQ_NODE_SETTING: {
        neu_req_node_setting_t *cmd   = (neu_req_node_setting_t *) &header[1];
        neu_resp_error_t        error = { 0 };

        error.error = neu_adapter_set_setting(adapter, cmd->setting);
        if (error.error == NEU_ERR_SUCCESS) {
            adapter_storage_setting(adapter->name, cmd->setting);
            // ownership of `cmd->setting` transfer
            notify_monitor(adapter, NEU_REQ_NODE_SETTING_EVENT, cmd);
        } else {
            free(cmd->setting);
        }

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
            strcpy(resp.node, adapter->name);
            reply(adapter, header, &resp);
        }

        break;
    }
    case NEU_REQ_GET_NODE_STATE: {
        neu_resp_get_node_state_t resp = { 0 };

        neu_metric_entry_t *e = NULL;
        if (NULL != adapter->metrics) {
            HASH_FIND_STR(adapter->metrics->entries, NEU_METRIC_LAST_RTT_MS, e);
        }
        resp.rtt     = NULL != e ? e->value : 0;
        resp.state   = neu_adapter_get_state(adapter);
        header->type = NEU_RESP_GET_NODE_STATE;
        neu_msg_exchange(header);
        reply(adapter, header, &resp);
        break;
    }
    case NEU_REQ_GET_GROUP: {
        neu_msg_exchange(header);

        if (adapter->module->type == NEU_NA_TYPE_DRIVER) {
            neu_resp_get_group_t resp = {
                .groups = neu_adapter_driver_get_group(
                    (neu_adapter_driver_t *) adapter)
            };
            header->type = NEU_RESP_GET_GROUP;
            reply(adapter, header, &resp);
        } else {
            neu_resp_error_t error = { .error = NEU_ERR_GROUP_NOT_ALLOW };

            header->type = NEU_RESP_ERROR;
            reply(adapter, header, &error);
        }
        break;
    }
    case NEU_REQ_GET_TAG: {
        neu_req_get_tag_t *cmd   = (neu_req_get_tag_t *) &header[1];
        neu_resp_error_t   error = { .error = 0 };
        UT_array *         tags  = NULL;

        if (adapter->module->type == NEU_NA_TYPE_DRIVER) {
            error.error = neu_adapter_driver_query_tag(
                (neu_adapter_driver_t *) adapter, cmd->group, cmd->name, &tags);
        } else {
            error.error = NEU_ERR_GROUP_NOT_ALLOW;
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
            if (adapter->module->type == NEU_NA_TYPE_DRIVER) {
                error.error = neu_adapter_driver_add_group(
                    (neu_adapter_driver_t *) adapter, cmd->group,
                    cmd->interval);
            } else {
                error.error = NEU_ERR_GROUP_NOT_ALLOW;
            }
        }

        if (error.error == NEU_ERR_SUCCESS) {
            adapter_storage_add_group(adapter->name, cmd->group, cmd->interval);
            notify_monitor(adapter, NEU_REQ_ADD_GROUP_EVENT, cmd);
        }

        neu_msg_exchange(header);
        header->type = NEU_RESP_ERROR;
        reply(adapter, header, &error);
        break;
    }
    case NEU_REQ_UPDATE_GROUP: {
        neu_req_update_group_t *cmd  = (neu_req_update_group_t *) &header[1];
        neu_resp_update_group_t resp = { 0 };

        if (adapter->module->type == NEU_NA_TYPE_DRIVER) {
            resp.error = neu_adapter_driver_update_group(
                (neu_adapter_driver_t *) adapter, cmd->group, cmd->new_name,
                cmd->interval);
        } else {
            resp.error = NEU_ERR_GROUP_NOT_ALLOW;
        }

        if (resp.error == NEU_ERR_SUCCESS) {
            adapter_storage_update_group(adapter->name, cmd->group,
                                         cmd->new_name, cmd->interval);
            notify_monitor(adapter, NEU_REQ_UPDATE_GROUP_EVENT, cmd);
        }

        strcpy(resp.driver, cmd->driver);
        strcpy(resp.group, cmd->group);
        strcpy(resp.new_name, cmd->new_name);
        header->type = NEU_RESP_UPDATE_GROUP;
        neu_msg_exchange(header);
        reply(adapter, header, &resp);
        break;
    }
    case NEU_REQ_DEL_GROUP: {
        neu_req_del_group_t *cmd   = (neu_req_del_group_t *) &header[1];
        neu_resp_error_t     error = { 0 };

        if (adapter->module->type == NEU_NA_TYPE_DRIVER) {
            error.error = neu_adapter_driver_del_group(
                (neu_adapter_driver_t *) adapter, cmd->group);
        } else {
            error.error = NEU_ERR_GROUP_NOT_ALLOW;
        }

        if (error.error == NEU_ERR_SUCCESS) {
            adapter_storage_del_group(cmd->driver, cmd->group);
            notify_monitor(adapter, NEU_REQ_DEL_GROUP_EVENT, cmd);
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

        if (0 == error.error) {
            notify_monitor(adapter, NEU_REQ_NODE_CTL_EVENT, cmd);
        }

        neu_msg_exchange(header);
        header->type = NEU_RESP_ERROR;
        reply(adapter, header, &error);
        break;
    }
    case NEU_REQ_NODE_RENAME: {
        neu_req_node_rename_t *cmd  = (neu_req_node_rename_t *) &header[1];
        neu_resp_node_rename_t resp = { 0 };
        resp.error = neu_adapter_rename(adapter, cmd->new_name);
        strcpy(header->receiver, header->sender);
        strcpy(header->sender, cmd->new_name);
        strcpy(resp.node, cmd->node);
        strcpy(resp.new_name, cmd->new_name);
        header->type = NEU_RESP_NODE_RENAME;
        reply(adapter, header, &resp);
        break;
    }
    case NEU_REQ_DEL_TAG: {
        neu_req_del_tag_t *cmd   = (neu_req_del_tag_t *) &header[1];
        neu_resp_error_t   error = { 0 };

        if (adapter->module->type == NEU_NA_TYPE_DRIVER) {
            for (int i = 0; i < cmd->n_tag; i++) {
                int ret = neu_adapter_driver_del_tag(
                    (neu_adapter_driver_t *) adapter, cmd->group, cmd->tags[i]);
                if (0 == ret) {
                    adapter_storage_del_tag(cmd->driver, cmd->group,
                                            cmd->tags[i]);
                }
            }
        } else {
            error.error = NEU_ERR_GROUP_NOT_ALLOW;
        }

        if (0 == error.error) {
            notify_monitor(adapter, NEU_REQ_DEL_TAG_EVENT, cmd);
        } else {
            for (uint16_t i = 0; i < cmd->n_tag; i++) {
                free(cmd->tags[i]);
            }
            free(cmd->tags);
        }

        neu_msg_exchange(header);
        header->type = NEU_RESP_ERROR;
        reply(adapter, header, &error);
        break;
    }
    case NEU_REQ_ADD_TAG: {
        neu_req_add_tag_t *cmd  = (neu_req_add_tag_t *) &header[1];
        neu_resp_add_tag_t resp = { 0 };

        if (adapter->module->type == NEU_NA_TYPE_DRIVER) {
            for (int i = 0; i < cmd->n_tag; i++) {
                int ret = neu_adapter_driver_validate_tag(
                    (neu_adapter_driver_t *) adapter, cmd->group,
                    &cmd->tags[i]);
                if (ret == 0) {
                    resp.index += 1;
                } else {
                    resp.error = ret;
                    break;
                }
            }
        } else {
            resp.error = NEU_ERR_GROUP_NOT_ALLOW;
        }

        if (resp.index > 0) {
            int ret = neu_adapter_driver_try_add_tag(
                (neu_adapter_driver_t *) adapter, cmd->group, cmd->tags,
                resp.index);
            if (ret != 0) {
                resp.index = 0;
                resp.error = ret;
            }
        }

        for (int i = 0; i < resp.index; i++) {
            int ret = neu_adapter_driver_add_tag(
                (neu_adapter_driver_t *) adapter, cmd->group, &cmd->tags[i]);
            if (ret != 0) {
                neu_adapter_driver_try_del_tag((neu_adapter_driver_t *) adapter,
                                               resp.index - i);
                resp.index = i;
                resp.error = ret;
                break;
            }
        }

        for (uint16_t i = resp.index; i < cmd->n_tag; i++) {
            neu_tag_fini(&cmd->tags[i]);
        }

        if (resp.index) {
            // we have added some tags, try to persist
            adapter_storage_add_tags(cmd->driver, cmd->group, cmd->tags,
                                     resp.index);
            cmd->n_tag = resp.index;
            notify_monitor(adapter, NEU_REQ_ADD_TAG_EVENT, cmd);
        } else {
            free(cmd->tags);
        }

        neu_msg_exchange(header);
        header->type = NEU_RESP_ADD_TAG;
        reply(adapter, header, &resp);
        break;
    }
    case NEU_REQ_UPDATE_TAG: {
        neu_req_update_tag_t *cmd  = (neu_req_update_tag_t *) &header[1];
        neu_resp_update_tag_t resp = { 0 };

        if (adapter->module->type == NEU_NA_TYPE_DRIVER) {
            for (int i = 0; i < cmd->n_tag; i++) {
                int ret = neu_adapter_driver_update_tag(
                    (neu_adapter_driver_t *) adapter, cmd->group,
                    &cmd->tags[i]);
                if (ret == 0) {
                    adapter_storage_update_tag(cmd->driver, cmd->group,
                                               &cmd->tags[i]);

                    resp.index += 1;
                } else {
                    resp.error = ret;
                    break;
                }
            }
        } else {
            resp.error = NEU_ERR_GROUP_NOT_ALLOW;
        }

        for (uint16_t i = resp.index; i < cmd->n_tag; i++) {
            neu_tag_fini(&cmd->tags[i]);
        }
        if (resp.index > 0) {
            cmd->n_tag = resp.index;
            notify_monitor(adapter, NEU_REQ_UPDATE_TAG_EVENT, cmd);
        } else {
            free(cmd->tags);
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
            nlog_notice("%s send uninit msg to %s", name, receiver);
        } else {
            nng_msg_free(uninit_msg);
            nlog_error("%s send uninit msg to %s failed", name, receiver);
        }
        break;
    }
    case NEU_REQ_ADD_NDRIVER_MAP: {
        break;
    }
    case NEU_REQ_DEL_NDRIVER_MAP: {
        break;
    }
    case NEU_REQ_UPDATE_NDRIVER_TAG_PARAM: {
        neu_req_update_ndriver_tag_param_t *cmd =
            (neu_req_update_ndriver_tag_param_t *) &header[1];
        neu_resp_update_tag_t resp = { 0 };

        for (uint16_t i = 0; i < cmd->n_tag; i++) {
            // TODO
            resp.index += 1;
        }

        neu_req_update_ndriver_tag_param_fini(cmd);

        neu_msg_exchange(header);
        header->type = NEU_RESP_UPDATE_TAG;
        reply(adapter, header, &resp);
        break;
    }
    case NEU_REQ_UPDATE_NDRIVER_TAG_INFO: {
        neu_req_update_ndriver_tag_info_t *cmd =
            (neu_req_update_ndriver_tag_info_t *) &header[1];
        neu_resp_update_tag_t resp = { 0 };

        for (uint16_t i = 0; i < cmd->n_tag; i++) {
            // TODO
            resp.index += 1;
        }

        neu_req_update_ndriver_tag_info_fini(cmd);

        neu_msg_exchange(header);
        header->type = NEU_RESP_UPDATE_TAG;
        reply(adapter, header, &resp);
        break;
    }
    case NEU_REQ_GET_NDRIVER_TAGS: {
        neu_req_get_ndriver_tags_t *cmd =
            (neu_req_get_ndriver_tags_t *) &header[1];
        neu_resp_error_t error = { .error = 0 };
        UT_array *       tags  = NULL;

        // TODO
        (void) cmd;
        utarray_new(tags, neu_ndriver_tag_get_icd());

        neu_msg_exchange(header);
        if (error.error != NEU_ERR_SUCCESS) {
            header->type = NEU_RESP_ERROR;
            reply(adapter, header, &error);
        } else {
            neu_resp_get_tag_t resp = { .tags = tags };
            header->type            = NEU_RESP_GET_NDRIVER_TAGS;
            reply(adapter, header, &resp);
        }

        break;
    }
    case NEU_REQ_UPDATE_LOG_LEVEL: {
        neu_resp_error_t error = { 0 };

        struct timeval tv = { 0 };
        gettimeofday(&tv, NULL);
        adapter->timestamp_lev = tv.tv_sec;

        neu_msg_exchange(header);
        header->type = NEU_RESP_ERROR;
        reply(adapter, header, &error);

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

    adapter->module->intf_funs->close(adapter->plugin);

    if (NULL != adapter->metrics) {
        neu_metrics_del_node(adapter);
        neu_metric_entry_t *e = NULL;
        HASH_LOOP(hh, adapter->metrics->entries, e)
        {
            neu_metrics_unregister_entry(e->name);
        }
        neu_node_metrics_free(adapter->metrics);
    }

    char *setting = NULL;
    if (adapter_load_setting(adapter->name, &setting) != 0) {
        UT_array *files = collect_logs("./logs", adapter->name);
        if (files != NULL) {
            if (0 == utarray_len(files)) {
                nlog_warn("directory logs contains no log files");
            }

            utarray_foreach(files, char **, file)
            {
                char path[NEU_NODE_NAME_LEN + 10] = { 0 };
                snprintf(path, sizeof(path), "./logs/%s", *file);
                if (remove(path) != 0) {
                    nlog_warn("rm %s file fail", path);
                }
            }
            utarray_free(files);
        }
    } else {
        free(setting);
    }

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

    stop_log_level_timer(adapter);

    nlog_notice("Stop the adapter(%s)", adapter->name);
    return 0;
}

int neu_adapter_start(neu_adapter_t *adapter)
{
    const neu_plugin_intf_funs_t *intf_funs = adapter->module->intf_funs;
    neu_err_code_e                error     = NEU_ERR_SUCCESS;

    switch (adapter->state) {
    case NEU_NODE_RUNNING_STATE_INIT:
        error = NEU_ERR_NODE_NOT_READY;
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
        adapter_storage_state(adapter->name, adapter->state);
    }

    return error;
}

int neu_adapter_start_single(neu_adapter_t *adapter)
{
    const neu_plugin_intf_funs_t *intf_funs = adapter->module->intf_funs;

    adapter->state = NEU_NODE_RUNNING_STATE_RUNNING;
    return intf_funs->start(adapter->plugin);
}

int neu_adapter_stop(neu_adapter_t *adapter)
{
    const neu_plugin_intf_funs_t *intf_funs = adapter->module->intf_funs;
    neu_err_code_e                error     = NEU_ERR_SUCCESS;

    switch (adapter->state) {
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
        adapter_storage_state(adapter->name, adapter->state);
        adapter_reset_metrics(adapter);
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
            neu_adapter_start(adapter);
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

int neu_adapter_register_group_metric(neu_adapter_t *adapter,
                                      const char *group_name, const char *name,
                                      const char *help, neu_metric_type_e type,
                                      uint64_t init)
{
    neu_group_metrics_t *group_metrics = NULL;

    if (NULL == adapter->metrics) {
        return -1;
    }

    if (0 > neu_metrics_register_entry(name, help, type)) {
        return -1;
    }

    HASH_FIND_STR(adapter->metrics->group_metrics, group_name, group_metrics);
    if (NULL == group_metrics) {
        group_metrics = calloc(1, sizeof(*group_metrics));
        if (NULL == group_metrics) {
            return -1;
        }
        group_metrics->name = strdup(group_name);
        if (NULL == group_metrics->name) {
            free(group_metrics);
            return -1;
        }
        HASH_ADD_STR(adapter->metrics->group_metrics, name, group_metrics);
    }

    if (0 > neu_metric_entries_add(&group_metrics->entries, name, help, type,
                                   init)) {
        return -1;
    }

    return 0;
}

int neu_adapter_update_group_metric(neu_adapter_t *adapter,
                                    const char *   group_name,
                                    const char *metric_name, uint64_t n)
{
    neu_metric_entry_t * entry         = NULL;
    neu_group_metrics_t *group_metrics = NULL;

    if (NULL == adapter->metrics) {
        return -1;
    }

    HASH_FIND_STR(adapter->metrics->group_metrics, group_name, group_metrics);
    if (NULL == group_metrics) {
        return -1;
    }

    HASH_FIND_STR(group_metrics->entries, metric_name, entry);
    if (NULL == entry) {
        return -1;
    }

    if (NEU_METRIC_TYPE_COUNTER == entry->type) {
        entry->value += n;
    } else {
        entry->value = n;
    }

    return 0;
}

int neu_adapter_metric_update_group_name(neu_adapter_t *adapter,
                                         const char *   group_name,
                                         const char *   new_group_name)
{
    neu_group_metrics_t *group_metrics = NULL;

    if (NULL == adapter->metrics) {
        return -1;
    }

    HASH_FIND_STR(adapter->metrics->group_metrics, group_name, group_metrics);
    if (NULL == group_metrics) {
        return -1;
    }

    char *name = strdup(new_group_name);
    if (NULL == name) {
        return -1;
    }

    HASH_DEL(adapter->metrics->group_metrics, group_metrics);
    free(group_metrics->name);
    group_metrics->name = name;
    HASH_ADD_STR(adapter->metrics->group_metrics, name, group_metrics);

    return 0;
}

void neu_adapter_del_group_metrics(neu_adapter_t *adapter,
                                   const char *   group_name)
{
    if (NULL == adapter->metrics) {
        return;
    }

    neu_group_metrics_t *gm = NULL;
    HASH_FIND_STR(adapter->metrics->group_metrics, group_name, gm);
    if (NULL != gm) {
        HASH_DEL(adapter->metrics->group_metrics, gm);
        neu_metric_entry_t *e = NULL;
        HASH_LOOP(hh, gm->entries, e) { neu_metrics_unregister_entry(e->name); }
        neu_group_metrics_free(gm);
    }
}

inline static void reply(neu_adapter_t *adapter, neu_reqresp_head_t *header,
                         void *data)
{
    nng_msg *msg = neu_msg_gen(header, data);

    int ret = nng_sendmsg(adapter->sock, msg, 0);
    if (ret != 0) {
        nng_msg_free(msg);
        nlog_warn("%s reply %s to %s, error: %s", header->sender,
                  neu_reqresp_type_string(header->type), header->receiver,
                  nng_strerror(ret));
    }
}

inline static void notify_monitor(neu_adapter_t *    adapter,
                                  neu_reqresp_type_e event, void *data)
{
    neu_reqresp_head_t header = {
        .receiver = "monitor",
        .type     = event,
    };

    strncpy(header.sender, adapter->name, NEU_NODE_NAME_LEN);

    nng_msg *msg = neu_msg_gen(&header, data);

    int ret = nng_sendmsg(adapter->sock, msg, 0);
    if (ret != 0) {
        nng_msg_free(msg);
        nlog_warn("notify %s of %s, error: %s", header.receiver,
                  neu_reqresp_type_string(header.type), nng_strerror(ret));
    }
}

static int level_check(void *usr_data)
{
    neu_adapter_t *adapter = (neu_adapter_t *) usr_data;

    if (0 != adapter->timestamp_lev) {
        struct timeval   tv      = { 0 };
        int64_t          diff    = 0;
        int64_t          delay_s = 600;
        zlog_category_t *ct      = zlog_get_category(adapter->name);

        gettimeofday(&tv, NULL);
        diff = tv.tv_sec - adapter->timestamp_lev;
        if (delay_s <= diff) {
            int ret = zlog_level_switch(ct, default_log_level);
            if (0 != ret) {
                nlog_error("Modify default log level fail, ret:%d", ret);
            }
        }
    }

    return 0;
}

void *neu_msg_gen(neu_reqresp_head_t *header, void *data)
{
    nng_msg *msg       = NULL;
    void *   body      = NULL;
    size_t   data_size = 0;

    switch (header->type) {
    case NEU_REQ_NODE_INIT:
    case NEU_REQ_NODE_UNINIT:
    case NEU_RESP_NODE_UNINIT:
        data_size = sizeof(neu_req_node_init_t);
        break;
    case NEU_RESP_ERROR:
        data_size = sizeof(neu_resp_error_t);
        break;
    case NEU_REQ_ADD_PLUGIN:
        data_size = sizeof(neu_req_add_plugin_t);
        break;
    case NEU_REQ_DEL_PLUGIN:
        data_size = sizeof(neu_req_del_plugin_t);
        break;
    case NEU_REQ_GET_PLUGIN:
        data_size = sizeof(neu_req_get_plugin_t);
        break;
    case NEU_RESP_GET_PLUGIN:
        data_size = sizeof(neu_resp_get_plugin_t);
        break;
    case NEU_REQ_ADD_TEMPLATE:
        data_size = sizeof(neu_req_add_template_t);
        break;
    case NEU_REQ_DEL_TEMPLATE:
        data_size = sizeof(neu_req_del_template_t);
        break;
    case NEU_REQ_GET_TEMPLATE:
        data_size = sizeof(neu_req_get_template_t);
        break;
    case NEU_RESP_GET_TEMPLATE:
        data_size = sizeof(neu_resp_get_template_t);
        break;
    case NEU_REQ_GET_TEMPLATES:
        data_size = sizeof(neu_req_get_templates_t);
        break;
    case NEU_RESP_GET_TEMPLATES:
        data_size = sizeof(neu_resp_get_templates_t);
        break;
    case NEU_REQ_ADD_TEMPLATE_GROUP:
        data_size = sizeof(neu_req_add_template_group_t);
        break;
    case NEU_REQ_DEL_TEMPLATE_GROUP:
        data_size = sizeof(neu_req_del_template_group_t);
        break;
    case NEU_REQ_UPDATE_TEMPLATE_GROUP:
        data_size = sizeof(neu_req_update_template_group_t);
        break;
    case NEU_REQ_GET_TEMPLATE_GROUP:
        data_size = sizeof(neu_req_get_template_group_t);
        break;
    case NEU_REQ_ADD_TEMPLATE_TAG:
        data_size = sizeof(neu_req_add_template_tag_t);
        break;
    case NEU_REQ_DEL_TEMPLATE_TAG:
        data_size = sizeof(neu_req_del_template_tag_t);
        break;
    case NEU_REQ_UPDATE_TEMPLATE_TAG:
        data_size = sizeof(neu_req_update_template_tag_t);
        break;
    case NEU_REQ_GET_TEMPLATE_TAG:
        data_size = sizeof(neu_req_get_template_tag_t);
        break;
    case NEU_REQ_INST_TEMPLATE:
        data_size = sizeof(neu_req_inst_template_t);
        break;
    case NEU_REQ_ADD_NODE:
    case NEU_REQ_ADD_NODE_EVENT:
        data_size = sizeof(neu_req_add_node_t);
        break;
    case NEU_REQ_UPDATE_NODE:
        data_size = sizeof(neu_req_update_node_t);
        break;
    case NEU_REQ_DEL_NODE:
    case NEU_REQ_DEL_NODE_EVENT:
        data_size = sizeof(neu_req_del_node_t);
        break;
    case NEU_REQ_GET_NODE:
        data_size = sizeof(neu_req_get_node_t);
        break;
    case NEU_RESP_GET_NODE:
        data_size = sizeof(neu_resp_get_node_t);
        break;
    case NEU_REQ_ADD_GROUP:
    case NEU_REQ_ADD_GROUP_EVENT:
        data_size = sizeof(neu_req_add_group_t);
        break;
    case NEU_REQ_UPDATE_GROUP:
    case NEU_REQ_UPDATE_GROUP_EVENT:
        data_size = sizeof(neu_req_update_group_t);
        break;
    case NEU_RESP_UPDATE_GROUP:
        data_size = sizeof(neu_resp_update_group_t);
        break;
    case NEU_REQ_DEL_GROUP:
    case NEU_REQ_DEL_GROUP_EVENT:
        data_size = sizeof(neu_req_del_group_t);
        break;
    case NEU_REQ_GET_DRIVER_GROUP:
    case NEU_REQ_GET_GROUP:
        data_size = sizeof(neu_req_get_group_t);
        break;
    case NEU_RESP_GET_GROUP:
        data_size = sizeof(neu_resp_get_group_t);
        break;
    case NEU_REQ_ADD_TAG:
    case NEU_REQ_ADD_TAG_EVENT:
        data_size = sizeof(neu_req_add_tag_t);
        break;
    case NEU_RESP_ADD_TAG:
    case NEU_RESP_ADD_TEMPLATE_TAG:
        data_size = sizeof(neu_resp_add_tag_t);
        break;
    case NEU_RESP_UPDATE_TAG:
    case NEU_RESP_UPDATE_TEMPLATE_TAG:
        data_size = sizeof(neu_resp_update_tag_t);
        break;
    case NEU_REQ_UPDATE_TAG:
    case NEU_REQ_UPDATE_TAG_EVENT:
        data_size = sizeof(neu_req_update_tag_t);
        break;
    case NEU_REQ_DEL_TAG:
    case NEU_REQ_DEL_TAG_EVENT:
        data_size = sizeof(neu_req_del_tag_t);
        break;
    case NEU_REQ_GET_TAG:
        data_size = sizeof(neu_req_get_tag_t);
        break;
    case NEU_RESP_GET_TAG:
    case NEU_RESP_GET_TEMPLATE_TAG:
        data_size = sizeof(neu_resp_get_tag_t);
        break;
    case NEU_REQ_SUBSCRIBE_GROUP:
    case NEU_REQ_UPDATE_SUBSCRIBE_GROUP:
        data_size = sizeof(neu_req_subscribe_t);
        break;
    case NEU_REQ_UNSUBSCRIBE_GROUP:
        data_size = sizeof(neu_req_unsubscribe_t);
        break;
    case NEU_REQ_SUBSCRIBE_GROUPS:
        data_size = sizeof(neu_req_subscribe_groups_t);
        break;
    case NEU_REQ_GET_SUBSCRIBE_GROUP:
    case NEU_REQ_GET_SUB_DRIVER_TAGS:
        data_size = sizeof(neu_req_get_subscribe_group_t);
        break;
    case NEU_RESP_GET_SUB_DRIVER_TAGS:
        data_size = sizeof(neu_resp_get_sub_driver_tags_t);
        break;
    case NEU_RESP_GET_SUBSCRIBE_GROUP:
        data_size = sizeof(neu_resp_get_subscribe_group_t);
        break;
    case NEU_REQ_NODE_SETTING:
    case NEU_REQ_NODE_SETTING_EVENT:
        data_size = sizeof(neu_req_node_setting_t);
        break;
    case NEU_REQ_GET_NODE_SETTING:
        data_size = sizeof(neu_req_get_node_setting_t);
        break;
    case NEU_RESP_GET_NODE_SETTING:
        data_size = sizeof(neu_resp_get_node_setting_t);
        break;
    case NEU_REQ_NODE_CTL:
    case NEU_REQ_NODE_CTL_EVENT:
        data_size = sizeof(neu_req_node_ctl_t);
        break;
    case NEU_REQ_NODE_RENAME:
        data_size = sizeof(neu_req_node_rename_t);
        break;
    case NEU_RESP_NODE_RENAME:
        data_size = sizeof(neu_resp_node_rename_t);
        break;
    case NEU_REQ_GET_NODE_STATE:
        data_size = sizeof(neu_req_get_node_state_t);
        break;
    case NEU_RESP_GET_NODE_STATE:
        data_size = sizeof(neu_resp_get_node_state_t);
        break;
    case NEU_REQ_GET_NODES_STATE:
        data_size = sizeof(neu_req_get_nodes_state_t);
        break;
    case NEU_REQRESP_NODES_STATE:
    case NEU_RESP_GET_NODES_STATE:
        data_size = sizeof(neu_resp_get_nodes_state_t);
        break;
    case NEU_REQ_READ_GROUP:
        data_size = sizeof(neu_req_read_group_t);
        break;
    case NEU_REQ_WRITE_TAG:
        data_size = sizeof(neu_req_write_tag_t);
        break;
    case NEU_REQ_WRITE_TAGS:
        data_size = sizeof(neu_req_write_tags_t);
        break;
    case NEU_RESP_READ_GROUP:
        data_size = sizeof(neu_resp_read_group_t);
        break;
    case NEU_REQRESP_TRANS_DATA: {
        neu_reqresp_trans_data_t *trans = (neu_reqresp_trans_data_t *) data;
        data_size                       = sizeof(neu_reqresp_trans_data_t) +
            trans->n_tag * sizeof(neu_resp_tag_value_meta_t);
        break;
    }
    case NEU_REQ_UPDATE_LICENSE:
        data_size = sizeof(neu_req_update_license_t);
        break;
    case NEU_REQRESP_NODE_DELETED:
        data_size = sizeof(neu_reqresp_node_deleted_t);
        break;
    case NEU_RESP_GET_DRIVER_GROUP:
        data_size = sizeof(neu_resp_get_driver_group_t);
        break;
    case NEU_REQ_ADD_NDRIVER_MAP:
    case NEU_REQ_DEL_NDRIVER_MAP:
        data_size = sizeof(neu_req_ndriver_map_t);
        break;
    case NEU_REQ_GET_NDRIVER_MAPS:
        data_size = sizeof(neu_req_get_ndriver_maps_t);
        break;
    case NEU_RESP_GET_NDRIVER_MAPS:
        data_size = sizeof(neu_resp_get_ndriver_maps_t);
        break;
    case NEU_REQ_UPDATE_NDRIVER_TAG_PARAM:
        data_size = sizeof(neu_req_update_ndriver_tag_param_t);
        break;
    case NEU_REQ_UPDATE_NDRIVER_TAG_INFO:
        data_size = sizeof(neu_req_update_ndriver_tag_info_t);
        break;
    case NEU_REQ_GET_NDRIVER_TAGS:
        data_size = sizeof(neu_req_get_ndriver_tags_t);
        break;
    case NEU_RESP_GET_NDRIVER_TAGS:
        data_size = sizeof(neu_resp_get_ndriver_tags_t);
        break;
    case NEU_REQ_UPDATE_LOG_LEVEL:
        data_size = sizeof(neu_req_update_log_level_t);
        break;
    default:
        assert(false);
        break;
    }

    nng_msg_alloc(&msg, sizeof(neu_reqresp_head_t) + data_size);
    body = nng_msg_body(msg);
    memcpy(body, header, sizeof(neu_reqresp_head_t));
    memcpy((uint8_t *) body + sizeof(neu_reqresp_head_t), data, data_size);
    return msg;
}