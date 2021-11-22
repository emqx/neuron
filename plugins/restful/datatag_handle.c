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

#include "neu_datatag_table.h"
#include "neu_log.h"
#include "neu_plugin.h"
#include "json/neu_json_error.h"
#include "json/neu_json_fn.h"
#include "json/neu_json_tag.h"

#include "handle.h"
#include "http.h"

#include "datatag_handle.h"

void handle_add_tags(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    REST_PROCESS_HTTP_REQUEST(
        aio, neu_json_add_tags_req_t, neu_json_decode_add_tags_req, {
            neu_taggrp_config_t *config = neu_system_find_group_config(
                plugin, req->node_id, req->group_config_name);
            neu_datatag_table_t *table =
                neu_system_get_datatags_table(plugin, req->node_id);

            if (table == NULL) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_NODE_NOT_EXIST, {
                    http_response(aio, error_code.error, result_error);
                });
                return;
            }

            if (config == NULL) {
                config = neu_taggrp_cfg_new(req->group_config_name);
                neu_taggrp_cfg_set_interval(config, 10000);
                neu_system_add_group_config(plugin, req->node_id, config);
            }

            vector_t *     ids  = neu_taggrp_cfg_get_datatag_ids(config);
            neu_err_code_e code = { 0 };

            for (int i = 0; i < req->n_tag; i++) {
                if (!neu_tag_check_attribute(req->tags[i].attribute)) {
                    code = NEU_ERR_TAG_ATTRIBUTE_NOT_SUPPORT;
                    continue;
                }

                neu_datatag_t *tag = calloc(1, sizeof(neu_datatag_t));

                tag->addr_str  = strdup(req->tags[i].address);
                tag->name      = strdup(req->tags[i].name);
                tag->type      = req->tags[i].type;
                tag->attribute = req->tags[i].attribute;

                neu_datatag_tbl_add(table, tag);
                vector_push_back(ids, &tag->id);
            }

            neu_taggrp_cfg_unanchor(config);

            NEU_JSON_RESPONSE_ERROR(
                code, { http_response(aio, code, result_error); });
        })
}

void handle_del_tags(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    REST_PROCESS_HTTP_REQUEST(
        aio, neu_json_del_tags_req_t, neu_json_decode_del_tags_req, {
            neu_taggrp_config_t *config = neu_system_find_group_config(
                plugin, req->node_id, req->group_config_name);
            neu_datatag_table_t *table =
                neu_system_get_datatags_table(plugin, req->node_id);

            if (config == NULL || table == NULL) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_NODE_NOT_EXIST, {
                    http_response(aio, error_code.error, result_error);
                });
                return;
            }

            for (int i = 0; i < req->n_id; i++) {
                if (neu_datatag_tbl_remove(table, req->ids[i]) == 0) {
                    vector_t *ids = neu_taggrp_cfg_get_datatag_ids(config);

                    VECTOR_FOR_EACH(ids, iter)
                    {
                        neu_datatag_id_t *id =
                            (neu_datatag_id_t *) iterator_get(&iter);
                        if (*id == req->ids[i]) {
                            iterator_erase(ids, &iter);
                            break;
                        }
                    }
                }
            }

            NEU_JSON_RESPONSE_ERROR(NEU_ERR_SUCCESS, {
                http_response(aio, NEU_ERR_SUCCESS, result_error);
            })
        })
}

void handle_update_tags(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    REST_PROCESS_HTTP_REQUEST(
        aio, neu_json_update_tags_req_t, neu_json_decode_update_tags_req, {
            neu_datatag_table_t *table =
                neu_system_get_datatags_table(plugin, req->node_id);
            if (table == NULL) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_NODE_NOT_EXIST, {
                    http_response(aio, error_code.error, result_error);
                });
                return;
            }

            neu_err_code_e code = { 0 };
            for (int i = 0; i < req->n_tag; i++) {
                neu_datatag_t *tag =
                    neu_datatag_tbl_get(table, req->tags[i].id);

                if (tag == NULL) {
                    code = NEU_ERR_TAG_NOT_EXIST;
                    continue;
                }

                if (!neu_tag_check_attribute(req->tags[i].attribute)) {
                    code = NEU_ERR_TAG_ATTRIBUTE_NOT_SUPPORT;
                    continue;
                }

                free(tag->addr_str);
                free(tag->name);
                tag->addr_str  = strdup(req->tags[i].address);
                tag->name      = strdup(req->tags[i].name);
                tag->type      = req->tags[i].type;
                tag->attribute = req->tags[i].attribute;

                neu_datatag_tbl_update(table, req->tags[i].id, tag);
            }

            NEU_JSON_RESPONSE_ERROR(
                code, { http_response(aio, code, result_error); });
        })
}

void handle_get_tags(nng_aio *aio)
{
    neu_plugin_t *plugin     = neu_rest_get_plugin();
    char *        result     = NULL;
    neu_node_id_t node_id    = { 0 };
    char *        s_grp_name = http_get_param(aio, "group_config_name");

    if (http_get_param_int(aio, "node_id", (int32_t *) &node_id) != 0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_PARAM_IS_WRONG, {
            http_response(aio, NEU_ERR_PARAM_IS_WRONG, result_error);
        })
        return;
    }

    neu_datatag_table_t *table = neu_system_get_datatags_table(plugin, node_id);
    if (table == NULL) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_NODE_NOT_EXIST, {
            http_response(aio, NEU_ERR_NODE_NOT_EXIST, result_error);
        })
        return;
    }

    vector_t grp_configs = neu_system_get_group_configs(plugin, node_id);
    neu_json_get_tags_resp_t tags_res = { 0 };
    int                      index    = 0;

    VECTOR_FOR_EACH(&grp_configs, iter)
    {
        neu_taggrp_config_t *config =
            *(neu_taggrp_config_t **) iterator_get(&iter);
        vector_t *ids = neu_taggrp_cfg_get_datatag_ids(config);
        if (s_grp_name != NULL &&
            strncmp(neu_taggrp_cfg_get_name(config), s_grp_name,
                    strlen(s_grp_name)) != 0) {
            continue;
        }

        tags_res.n_tag += ids->size;
    }

    tags_res.tags =
        calloc(tags_res.n_tag, sizeof(neu_json_get_tags_resp_tag_t));

    VECTOR_FOR_EACH(&grp_configs, iter)
    {
        neu_taggrp_config_t *config =
            *(neu_taggrp_config_t **) iterator_get(&iter);
        const char *group_name = neu_taggrp_cfg_get_name(config);

        if (s_grp_name != NULL &&
            strncmp(group_name, s_grp_name, strlen(s_grp_name)) != 0) {
            continue;
        }

        vector_t *ids = neu_taggrp_cfg_get_datatag_ids(config);

        VECTOR_FOR_EACH(ids, iter_id)
        {
            neu_datatag_id_t *id  = (neu_datatag_id_t *) iterator_get(&iter_id);
            neu_datatag_t *   tag = neu_datatag_tbl_get(table, *id);

            tags_res.tags[index].id                = *id;
            tags_res.tags[index].name              = tag->name;
            tags_res.tags[index].group_config_name = (char *) group_name;
            tags_res.tags[index].type              = tag->type;
            tags_res.tags[index].attribute         = tag->attribute;
            tags_res.tags[index].address           = tag->addr_str;

            index += 1;
        }
    }

    neu_json_encode_by_fn(&tags_res, neu_json_encode_get_tags_resp, &result);

    http_ok(aio, result);

    vector_uninit(&grp_configs);
    free(result);
    free(tags_res.tags);
}