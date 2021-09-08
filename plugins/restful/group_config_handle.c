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

#include "neu_log.h"
#include "neu_plugin.h"
#include "parser/neu_json_group_config.h"
#include "parser/neu_json_parser.h"

#include "handle.h"
#include "http.h"

#include "group_config_handle.h"

void handle_add_group_config(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    REST_PROCESS_HTTP_REQUEST(aio, struct neu_parse_add_group_config_req, {
        neu_taggrp_config_t *gconfig = neu_taggrp_cfg_new(req->config);
        neu_taggrp_cfg_set_interval(gconfig, req->read_interval);
        intptr_t err = neu_system_add_group_config(plugin, req->src_node_id,
                                                   req->dst_node_id, gconfig);
        if (err != 0) {
            http_bad_request(aio, "{\"error\": 1}");
        } else {
            http_ok(aio, "{\"error\": 0}");
        }
    })
}

void handle_del_group_config(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    REST_PROCESS_HTTP_REQUEST(aio, struct neu_parse_delete_group_config_req, {
        neu_taggrp_config_t *config =
            neu_rest_find_config(plugin, req->node_id, req->config);
        neu_datatag_table_t *table =
            neu_system_get_datatags_table(plugin, req->node_id);
        intptr_t err = 0;

        if (config == NULL || table == NULL) {
            http_not_found(aio, "{\"error\" : 1}");
        } else {
            vector_t *ids = neu_taggrp_cfg_get_datatag_ids(config);

            VECTOR_FOR_EACH(ids, iter)
            {
                neu_datatag_id_t *id = (neu_datatag_id_t *) iterator_get(&iter);
                neu_datatag_tbl_remove(table, *id);
            }

            err =
                neu_system_del_group_config(plugin, req->node_id, req->config);
            if (err != 0) {
                http_bad_request(aio, "{\"error\": 1}");
            } else {
                http_ok(aio, "{\"error\": 0}");
            }
        }
    })
}

void handle_update_group_config(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    REST_PROCESS_HTTP_REQUEST(aio, struct neu_parse_update_group_config_req, {
        neu_taggrp_config_t *config =
            neu_rest_find_config(plugin, req->src_node_id, req->config);

        if (config == NULL) {
            http_not_found(aio, "{\"error\": 1}");
        } else {
            neu_taggrp_cfg_set_interval(config, req->read_interval);
            intptr_t err = neu_system_update_group_config(
                plugin, req->src_node_id, req->dst_node_id, config);
            if (err != 0) {
                http_bad_request(aio, "{\"error\": 1}");
            } else {
                http_ok(aio, "{\"error\": 0}");
            }
        }
    })
}

void handle_get_group_config(nng_aio *aio)
{
    neu_plugin_t *plugin    = neu_rest_get_plugin();
    char *        result    = NULL;
    char *        s_node_id = http_get_param(aio, "node_id");
    neu_node_id_t node_id   = (neu_node_id_t) atoi(s_node_id);
    struct neu_parse_get_group_config_res gconfig_res = { 0 };
    int                                   index       = 0;
    vector_t gconfigs = neu_system_get_group_configs(plugin, node_id);

    gconfig_res.n_config = gconfigs.size;
    gconfig_res.rows =
        calloc(gconfig_res.n_config,
               sizeof(struct neu_parse_get_group_config_res_row));

    VECTOR_FOR_EACH(&gconfigs, iter)
    {
        neu_taggrp_config_t *config =
            *(neu_taggrp_config_t **) iterator_get(&iter);

        gconfig_res.rows[index].name = (char *) neu_taggrp_cfg_get_name(config);
        gconfig_res.rows[index].read_interval =
            neu_taggrp_cfg_get_interval(config);
        gconfig_res.rows[index].tag_count =
            neu_taggrp_cfg_get_datatag_ids(config)->size;
        gconfig_res.rows[index].pipe_count =
            neu_taggrp_cfg_get_subpipes(config)->size;

        index += 1;
    }

    neu_parse_encode_get_group_config_res(&gconfig_res, &result);

    http_ok(aio, result);

    vector_uninit(&gconfigs);
    free(result);
    free(gconfig_res.rows);
}
