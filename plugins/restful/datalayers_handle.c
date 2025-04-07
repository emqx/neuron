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
#include <stdlib.h>

#include "parser/neu_json_datalayers.h"
#include "plugin.h"
#include "json/neu_json_error.h"
#include "json/neu_json_fn.h"

#include "handle.h"
#include "utils/http.h"

#include "datalayers_handle.h"

void handle_datalayers_get_groups(nng_aio *aio)
{
    int                           ret    = 0;
    neu_plugin_t *                plugin = neu_rest_get_plugin();
    neu_req_get_subscribe_group_t cmd    = { 0 };
    neu_reqresp_head_t            header = {
        .ctx             = aio,
        .type            = NEU_REQ_GET_DATALAYERS_GROUPS,
        .otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM,
    };

    NEU_VALIDATE_JWT(aio);

    // required parameter
    snprintf(cmd.app, sizeof(cmd.app), "%s", "datalayers");

    // optional parameter
    ret = neu_http_get_param_str(aio, "driver", cmd.driver, sizeof(cmd.driver));
    if (-1 == ret || (size_t) ret == sizeof(cmd.driver)) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_PARAM_IS_WRONG, {
            neu_http_response(aio, error_code.error, result_error);
        })
        return;
    }

    // optional parameter
    ret = neu_http_get_param_str(aio, "group", cmd.group, sizeof(cmd.group));
    if (-1 == ret || (size_t) ret == sizeof(cmd.driver)) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_PARAM_IS_WRONG, {
            neu_http_response(aio, error_code.error, result_error);
        })
        return;
    }

    ret = neu_plugin_op(plugin, header, &cmd);
    if (ret != 0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
            neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
        });
    }
}

void handle_datalayers_get_groups_resp(nng_aio *                       aio,
                                       neu_resp_get_subscribe_group_t *groups)
{
    char *                        result          = NULL;
    neu_json_get_subscribe_resp_t sub_grp_configs = { 0 };

    sub_grp_configs.n_group = utarray_len(groups->groups);
    sub_grp_configs.groups  = calloc(
        sub_grp_configs.n_group, sizeof(neu_json_get_subscribe_resp_group_t));

    utarray_foreach(groups->groups, neu_resp_subscribe_info_t *, group)
    {
        int index = utarray_eltidx(groups->groups, group);
        sub_grp_configs.groups[index].driver      = group->driver;
        sub_grp_configs.groups[index].group       = group->group;
        sub_grp_configs.groups[index].params      = group->params;
        sub_grp_configs.groups[index].static_tags = group->static_tags;
    }

    neu_json_encode_by_fn(&sub_grp_configs,
                          neu_json_encode_datalayers_get_groups_resp, &result);

    neu_http_ok(aio, result);
    free(result);
    free(sub_grp_configs.groups);
    utarray_free(groups->groups);
}

void handle_datalayers_get_tags(nng_aio *aio)
{
    int                           ret    = 0;
    neu_plugin_t *                plugin = neu_rest_get_plugin();
    neu_req_get_sub_driver_tags_t cmd    = { 0 };
    neu_reqresp_head_t            header = {
        .ctx             = aio,
        .type            = NEU_REQ_GET_DATALAYERS_TAGS,
        .otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM,
    };

    NEU_VALIDATE_JWT(aio);

    // required parameter
    snprintf(cmd.app, sizeof(cmd.app), "%s", "datalayers");

    ret = neu_plugin_op(plugin, header, &cmd);
    if (ret != 0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
            neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
        });
    }
}

void handle_datalayers_get_tags_resp(nng_aio *                       aio,
                                     neu_resp_get_sub_driver_tags_t *tags)
{
    char *result = NULL;
    int   ret    = neu_json_encode_by_fn(
        tags, neu_json_encode_datalayers_get_tags_resp, &result);

    if (ret != 0 || result == NULL) {
        neu_http_response(
            aio, 500, "{\"error\":4200,\"reason\":\"failed to encode tags\"}");
        return;
    }

    neu_http_ok(aio, result);

    free(result);
    if (tags) {
        utarray_foreach(tags->infos, neu_resp_get_sub_driver_tags_info_t *,
                        info)
        {
            utarray_free(info->tags);
        }
        utarray_free(tags->infos);
    }
}