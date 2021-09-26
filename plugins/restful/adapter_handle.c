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
#include "parser/neu_json_fn.h"
#include "parser/neu_json_node.h"

#include "handle.h"
#include "http.h"

#include "adapter_handle.h"

void handle_add_adapter(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    REST_PROCESS_HTTP_REQUEST(
        aio, neu_parse_add_node_req_t, neu_parse_decode_add_node, {
            intptr_t err = neu_system_add_node(
                plugin, req->node_type, req->node_name, req->plugin_name);
            if (err != 0) {
                http_bad_request(aio, "{\"error\": 1}");
            } else {
                http_ok(aio, "{\"error\": 0}");
            }
        })
}

void handle_del_adapter(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    REST_PROCESS_HTTP_REQUEST(
        aio, neu_parse_del_node_req_t, neu_parse_decode_del_node, {
            intptr_t err = neu_system_del_node(plugin, req->node_id);
            if (err != 0) {
                http_bad_request(aio, "{\"error\": 1}");
            } else {
                http_ok(aio, "{\"error\": 0}");
            }
        })
}

void handle_update_adapter(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    REST_PROCESS_HTTP_REQUEST(
        aio, neu_parse_update_node_req_t, neu_parse_decode_update_node, {
            intptr_t err = neu_system_update_node(
                plugin, req->node_type, req->node_name, req->plugin_name);
            if (err != 0) {
                http_bad_request(aio, "{\"error\": 1}");
            } else {
                http_ok(aio, "{\"error\": 0}");
            }
        })
}

void handle_get_adapter(nng_aio *aio)
{
    neu_plugin_t *plugin      = neu_rest_get_plugin();
    char *        result      = NULL;
    char *        s_node_type = http_get_param(aio, "type");

    if (s_node_type == NULL) {
        http_bad_request(aio, "{\"error\": 1}");
        return;
    }

    neu_node_type_e           node_type = (neu_node_type_e) atoi(s_node_type);
    neu_parse_get_nodes_res_t nodes_res = { 0 };
    int                       index     = 0;
    vector_t                  nodes = neu_system_get_nodes(plugin, node_type);

    nodes_res.n_node = nodes.size;
    nodes_res.nodes =
        calloc(nodes_res.n_node, sizeof(neu_parse_get_nodes_res_node_t));

    VECTOR_FOR_EACH(&nodes, iter)
    {
        neu_node_info_t *info = (neu_node_info_t *) iterator_get(&iter);

        nodes_res.nodes[index].id   = info->node_id;
        nodes_res.nodes[index].name = info->node_name;
        index += 1;
    }

    neu_json_encode_by_fn(&nodes_res, neu_parse_encode_get_nodes, &result);

    http_ok(aio, result);

    free(result);
    free(nodes_res.nodes);
    vector_uninit(&nodes);
}