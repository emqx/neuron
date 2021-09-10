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

#include "vector.h"

#include "neu_plugin.h"
#include "parser/neu_json_parser.h"
#include "parser/neu_json_plugin.h"

#include "handle.h"
#include "http.h"

#include "plugin_handle.h"

void handle_add_plugin(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    REST_PROCESS_HTTP_REQUEST(aio, struct neu_parse_add_plugin_req, {
        intptr_t err =
            neu_system_add_plugin(plugin, req->kind, req->node_type,
                                  req->plugin_name, req->plugin_lib_name);
        if (err != 0) {
            http_bad_request(aio, "{\"error\": 1}");
        } else {
            http_ok(aio, "{\"error\": 0}");
        }
    })
}

void handle_del_plugin(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    REST_PROCESS_HTTP_REQUEST(aio, struct neu_parse_delete_plugin_req, {
        plugin_id_t id;
        id.id_val    = req->plugin_id;
        intptr_t err = neu_system_del_plugin(plugin, id);
        if (err != 0) {
            http_bad_request(aio, "{\"error\": 1}");
        } else {
            http_ok(aio, "{\"error\": 0}");
        }
    })
}

void handle_update_plugin(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    REST_PROCESS_HTTP_REQUEST(aio, struct neu_parse_update_plugin_req, {
        intptr_t err =
            neu_system_update_plugin(plugin, req->kind, req->node_type,
                                     req->plugin_name, req->plugin_lib_name);
        if (err != 0) {
            http_bad_request(aio, "{\"error\": 1}");
        } else {
            http_ok(aio, "{\"error\": 0}");
        }
    })
}

void handle_get_plugin(nng_aio *aio)
{
    neu_plugin_t *                  plugin     = neu_rest_get_plugin();
    char *                          result     = NULL;
    struct neu_parse_get_plugin_res plugin_res = { 0 };
    int                             index      = 0;

    vector_t plugin_libs = neu_system_get_plugin(plugin);

    plugin_res.n_plugin    = plugin_libs.size;
    plugin_res.plugin_libs = calloc(
        plugin_res.n_plugin, sizeof(struct neu_parse_get_plugin_res_libs));

    VECTOR_FOR_EACH(&plugin_libs, iter)
    {
        plugin_lib_info_t *info = (plugin_lib_info_t *) iterator_get(&iter);

        plugin_res.plugin_libs[index].node_type   = info->node_type;
        plugin_res.plugin_libs[index].plugin_id   = info->plugin_id.id_val;
        plugin_res.plugin_libs[index].kind        = info->plugin_kind;
        plugin_res.plugin_libs[index].plugin_name = (char *) info->plugin_name;
        plugin_res.plugin_libs[index].plugin_lib_name =
            (char *) info->plugin_lib_name;

        index += 1;
    }

    neu_parse_encode_get_plugin_res(&plugin_res, &result);
    http_ok(aio, result);
    free(result);
    free(plugin_res.plugin_libs);

    vector_uninit(&plugin_libs);
}