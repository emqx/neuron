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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <nng/nng.h>
#include <nng/supplemental/http/http.h>

#include "adapter_handle.h"
#include "argparse.h"
#include "datatag_handle.h"
#include "define.h"
#include "global_config_handle.h"
#include "group_config_handle.h"
#include "handle.h"
#include "ndriver_handle.h"
#include "plugin_handle.h"
#include "rest.h"
#include "rw_handle.h"
#include "template_handle.h"
#include "utils/http.h"
#include "utils/log.h"
#include "utils/neu_jwt.h"
#include "json/neu_json_fn.h"

#define neu_plugin_module default_dashboard_plugin_module

struct neu_plugin {
    neu_plugin_common_t    common;
    nng_http_server *      server;
    neu_rest_handle_ctx_t *handle_ctx;
};

/**
 * @brief 初始化HTTP服务器
 *
 * 此函数创建并初始化NNG HTTP服务器，绑定到指定的URL地址和端口。
 *
 * @return 成功返回服务器指针，失败返回NULL
 */
static nng_http_server *server_init()
{
    nng_url *        url;
    nng_http_server *server;

    nlog_notice("bind url: %s", host_port);

    int ret = nng_url_parse(&url, host_port);
    if (ret != 0) {
        nng_url_free(url);
        return NULL;
    }

    ret = nng_http_server_hold(&server, url);
    if (ret != 0) {
        nlog_error("rest api server bind error: %d", ret);
        return NULL;
    }
    nng_url_free(url);

    return server;
}

/**
 * @brief 打开仪表板插件
 *
 * 此函数创建并初始化仪表板插件实例，设置HTTP处理器和CORS处理器，
 * 启动HTTP服务器。
 *
 * @return 成功返回插件实例指针，失败返回NULL
 */
static neu_plugin_t *dashb_plugin_open(void)
{
    int                            rv;
    neu_plugin_t *                 plugin    = calloc(1, sizeof(neu_plugin_t));
    uint32_t                       n_handler = 0;
    const struct neu_http_handler *rest_handlers = NULL;
    const struct neu_http_handler *cors          = NULL;

    neu_plugin_common_init(&plugin->common);

    plugin->handle_ctx = neu_rest_init_ctx(plugin);

    plugin->server = server_init();

    if (plugin->server == NULL) {
        nlog_error("Failed to create RESTful server");
        goto server_init_fail;
    }

    neu_rest_handler(&rest_handlers, &n_handler);
    for (uint32_t i = 0; i < n_handler; i++) {
        neu_http_add_handler(plugin->server, &rest_handlers[i]);
    }
    neu_rest_api_cors_handler(&cors, &n_handler);
    for (uint32_t i = 0; i < n_handler; i++) {
        neu_http_add_handler(plugin->server, &cors[i]);
    }
    if ((rv = nng_http_server_start(plugin->server)) != 0) {
        nlog_error("Failed to start api server, error=%d", rv);
        goto server_init_fail;
    }

    nlog_notice("Success to create plugin: %s", neu_plugin_module.module_name);
    return plugin;

server_init_fail:
    if (plugin->server != NULL) {
        nng_http_server_release(plugin->server);
    }
    neu_rest_free_ctx(plugin->handle_ctx);
    free(plugin);
    return NULL;
}

/**
 * @brief 关闭仪表板插件
 *
 * 此函数停止并释放HTTP服务器，清理插件资源。
 *
 * @param plugin 插件实例指针
 * @return 成功返回0，失败返回错误码
 */
static int dashb_plugin_close(neu_plugin_t *plugin)
{
    int rv = 0;

    nng_http_server_stop(plugin->server);
    nng_http_server_release(plugin->server);

    free(plugin);
    nlog_notice("Success to free plugin: %s", neu_plugin_module.module_name);
    return rv;
}

/**
 * @brief 初始化仪表板插件
 *
 * 此函数执行插件的初始化操作，包括JWT系统初始化。
 *
 * @param plugin 插件实例指针
 * @param load 是否为加载时调用
 * @return 成功返回0，失败返回错误码
 */
static int dashb_plugin_init(neu_plugin_t *plugin, bool load)
{
    (void) load;
    int rv = 0;

    (void) plugin;

    rv = neu_jwt_init(g_config_dir);

    nlog_notice("Initialize plugin: %s", neu_plugin_module.module_name);
    return rv;
}

/**
 * @brief 卸载仪表板插件
 *
 * 此函数执行插件的卸载操作，清理插件资源。
 *
 * @param plugin 插件实例指针
 * @return 成功返回0，失败返回错误码
 */
static int dashb_plugin_uninit(neu_plugin_t *plugin)
{
    int rv = 0;

    (void) plugin;

    nlog_notice("Uninitialize plugin: %s", neu_plugin_module.module_name);
    return rv;
}

/**
 * @brief 配置仪表板插件
 *
 * 此函数应用新的插件配置。
 *
 * @param plugin 插件实例指针
 * @param configs 配置字符串
 * @return 成功返回0，失败返回错误码
 */
static int dashb_plugin_config(neu_plugin_t *plugin, const char *configs)
{
    int rv = 0;

    (void) plugin;
    (void) configs;

    nlog_notice("config plugin: %s", neu_plugin_module.module_name);
    return rv;
}

/**
 * @brief 处理插件请求
 *
 * 此函数处理各种插件请求类型，并调用相应的处理函数生成HTTP响应。
 *
 * @param plugin 插件实例指针
 * @param header 请求头信息
 * @param data 请求数据
 * @return 成功返回0，失败返回错误码
 */
static int dashb_plugin_request(neu_plugin_t *      plugin,
                                neu_reqresp_head_t *header, void *data)
{
    (void) plugin;

    if (header->ctx && nng_aio_get_input(header->ctx, 3)) {
        // catch all response messages for global config request
        handle_global_config_resp(header->ctx, header->type, data);
        return 0;
    }

    switch (header->type) {
    case NEU_RESP_ERROR: {
        neu_resp_error_t *error = (neu_resp_error_t *) data;
        NEU_JSON_RESPONSE_ERROR(error->error, {
            neu_http_response(header->ctx, error->error, result_error);
        });
        break;
    }
    case NEU_RESP_GET_PLUGIN:
        handle_get_plugin_resp(header->ctx, (neu_resp_get_plugin_t *) data);
        break;
    case NEU_RESP_GET_NODE:
        handle_get_adapter_resp(header->ctx, (neu_resp_get_node_t *) data);
        break;
    case NEU_RESP_GET_GROUP:
        handle_get_group_resp(header->ctx, (neu_resp_get_group_t *) data);
        break;
    case NEU_RESP_GET_DRIVER_GROUP:
        handle_get_driver_group_resp(header->ctx,
                                     (neu_resp_get_driver_group_t *) data);
        break;
    case NEU_RESP_ADD_TAG:
    case NEU_RESP_ADD_TEMPLATE_TAG:
        handle_add_tags_resp(header->ctx, (neu_resp_add_tag_t *) data);
        break;
    case NEU_RESP_UPDATE_TAG:
    case NEU_RESP_UPDATE_TEMPLATE_TAG:
        handle_update_tags_resp(header->ctx, (neu_resp_update_tag_t *) data);
        break;
    case NEU_RESP_GET_TAG:
    case NEU_RESP_GET_TEMPLATE_TAG:
        handle_get_tags_resp(header->ctx, (neu_resp_get_tag_t *) data);
        break;
    case NEU_RESP_GET_SUBSCRIBE_GROUP:
        handle_grp_get_subscribe_resp(header->ctx,
                                      (neu_resp_get_subscribe_group_t *) data);
        break;
    case NEU_RESP_GET_NODE_SETTING:
        handle_get_node_setting_resp(header->ctx,
                                     (neu_resp_get_node_setting_t *) data);
        break;
    case NEU_RESP_GET_NODES_STATE:
        handle_get_nodes_state_resp(header->ctx,
                                    (neu_resp_get_nodes_state_t *) data);
        break;
    case NEU_RESP_GET_NODE_STATE:
        handle_get_node_state_resp(header->ctx,
                                   (neu_resp_get_node_state_t *) data);
        break;
    case NEU_RESP_READ_GROUP:
        handle_read_resp(header->ctx, (neu_resp_read_group_t *) data);
        break;
    case NEU_RESP_GET_TEMPLATE:
        handle_get_template_resp(header->ctx, (neu_resp_get_template_t *) data);
        break;
    case NEU_RESP_GET_TEMPLATES:
        handle_get_templates_resp(header->ctx,
                                  (neu_resp_get_templates_t *) data);
        break;
    case NEU_RESP_GET_NDRIVER_MAPS:
        handle_get_ndriver_maps_resp(header->ctx,
                                     (neu_resp_get_ndriver_maps_t *) data);
        break;
    case NEU_RESP_GET_NDRIVER_TAGS:
        handle_get_ndriver_tags_resp(header->ctx,
                                     (neu_resp_get_ndriver_tags_t *) data);
        break;
    default:
        nlog_fatal("recv unhandle msg: %s",
                   neu_reqresp_type_string(header->type));
        assert(false);
        break;
    }
    return 0;
}

/**
 * @brief 启动仪表板插件
 *
 * 此函数启动插件的运行状态。
 *
 * @param plugin 插件实例指针
 * @return 成功返回0，失败返回错误码
 */
static int dashb_plugin_start(neu_plugin_t *plugin)
{
    (void) plugin;
    return 0;
}

/**
 * @brief 停止仪表板插件
 *
 * 此函数停止插件的运行状态。
 *
 * @param plugin 插件实例指针
 * @return 成功返回0，失败返回错误码
 */
static int dashb_plugin_stop(neu_plugin_t *plugin)
{
    (void) plugin;
    return 0;
}

static const neu_plugin_intf_funs_t plugin_intf_funs = {
    .open    = dashb_plugin_open,
    .close   = dashb_plugin_close,
    .init    = dashb_plugin_init,
    .uninit  = dashb_plugin_uninit,
    .start   = dashb_plugin_start,
    .stop    = dashb_plugin_stop,
    .setting = dashb_plugin_config,
    .request = dashb_plugin_request,
};

#define DEFAULT_DASHBOARD_PLUGIN_DESCR \
    "A restful plugin for dashboard webserver"

const neu_plugin_module_t default_dashboard_plugin_module = {
    .version      = NEURON_PLUGIN_VER_1_0,
    .module_name  = "neuron-default-dashboard",
    .module_descr = DEFAULT_DASHBOARD_PLUGIN_DESCR,
    .intf_funs    = &plugin_intf_funs,
    .kind         = NEU_PLUGIN_KIND_SYSTEM,
    .type         = NEU_NA_TYPE_APP,
};
