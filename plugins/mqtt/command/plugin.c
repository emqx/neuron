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

#include <stdlib.h>

#include "plugin.h"

char *command_plugin_get(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt,
                         neu_json_get_plugin_req_t *req)
{
    log_info("Get plugin list uuid:%s, plugin id:%d", mqtt->uuid, req->id);
    neu_json_get_plugin_resp_t res         = { 0 };
    int                        index       = 0;
    vector_t                   plugin_libs = neu_system_get_plugin(plugin);

    res.n_plugin_lib = plugin_libs.size;
    if (0 < req->id) {
        res.n_plugin_lib = 1;
    }

    res.plugin_libs =
        calloc(res.n_plugin_lib, sizeof(neu_json_get_plugin_resp_plugin_lib_t));

    VECTOR_FOR_EACH(&plugin_libs, iter)
    {
        plugin_lib_info_t *info = (plugin_lib_info_t *) iterator_get(&iter);

        if (0 < req->id && (req->id != info->plugin_id.id_val)) {
            continue;
        }

        res.plugin_libs[index].node_type = info->node_type;
        res.plugin_libs[index].id        = info->plugin_id.id_val;
        res.plugin_libs[index].kind      = info->plugin_kind;
        res.plugin_libs[index].name      = (char *) info->plugin_name;
        res.plugin_libs[index].lib_name  = (char *) info->plugin_lib_name;

        index += 1;
    }

    vector_uninit(&plugin_libs);

    char *result = NULL;
    neu_json_encode_with_mqtt(&res, neu_json_encode_get_plugin_resp, mqtt,
                              neu_json_encode_mqtt_resp, &result);
    return result;
}

char *command_plugin_add(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt,
                         neu_json_add_plugin_req_t *req)
{
    log_info("Add plugin uuid:%s, node name:%s", mqtt->uuid, req->name);
    neu_json_error_resp_t error = { 0 };
    error.error  = neu_system_add_plugin(plugin, req->kind, req->node_type,
                                        req->name, req->lib_name);
    char *result = NULL;
    neu_json_encode_with_mqtt(&error, neu_json_encode_error_resp, mqtt,
                              neu_json_encode_mqtt_resp, &result);
    return result;
}

char *command_plugin_update(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt,
                            neu_json_update_plugin_req_t *req)
{
    log_info("Update plugin uuid:%s, node name:%s", mqtt->uuid, req->name);
    neu_json_error_resp_t error = { 0 };
    error.error  = neu_system_update_plugin(plugin, req->kind, req->node_type,
                                           req->name, req->lib_name);
    char *result = NULL;
    neu_json_encode_with_mqtt(&error, neu_json_encode_error_resp, mqtt,
                              neu_json_encode_mqtt_resp, &result);
    return result;
}

char *command_plugin_delete(neu_plugin_t *plugin, neu_json_mqtt_t *mqtt,
                            neu_json_del_plugin_req_t *req)
{
    log_info("Delete plugin uuid:%s, plugin id:%d", mqtt->uuid, req->id);
    neu_json_error_resp_t error = { 0 };
    plugin_id_t           id    = { 0 };
    id.id_val                   = req->id;
    error.error                 = neu_system_del_plugin(plugin, id);
    char *result                = NULL;
    neu_json_encode_with_mqtt(&error, neu_json_encode_error_resp, mqtt,
                              neu_json_encode_mqtt_resp, &result);
    return result;
}