/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2023 EMQ Technologies Co., Ltd All rights reserved.
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

#include "define.h"
#include "errcodes.h"
#include "tag.h"
#include "utils/http.h"
#include "utils/http_handler.h"
#include "utils/log.h"
#include "json/neu_json_error.h"
#include "json/neu_json_fn.h"

#include "parser/neu_json_template.h"

#include "handle.h"

static int set_tag_by_json(neu_datatag_t *tag, neu_json_tag_t *json_tag)
{
    if (strlen(json_tag->name) >= NEU_TAG_NAME_LEN) {
        return NEU_ERR_TAG_NAME_TOO_LONG;
    }

    if (strlen(json_tag->address) >= NEU_TAG_ADDRESS_LEN) {
        return NEU_ERR_TAG_ADDRESS_TOO_LONG;
    }

    if (NULL == json_tag->description &&
        NULL == (json_tag->description = strdup(""))) {
        return NEU_ERR_EINTERNAL;
    }

    tag->name        = json_tag->name;
    tag->type        = json_tag->type;
    tag->address     = json_tag->address;
    tag->attribute   = json_tag->attribute;
    tag->precision   = json_tag->precision;
    tag->decimal     = json_tag->decimal;
    tag->description = json_tag->description;

    if (NEU_ATTRIBUTE_STATIC & json_tag->attribute) {
        neu_tag_set_static_value_json(tag, json_tag->t, &json_tag->value);
    }

    return 0;
}

static int move_template_json(neu_req_add_template_t *cmd,
                              neu_json_template_t *   req)
{
    int ret = 0;

    if (strlen(req->name) >= NEU_NODE_NAME_LEN) {
        return NEU_ERR_NODE_NAME_TOO_LONG;
    }

    if (strlen(req->plugin) >= NEU_PLUGIN_NAME_LEN) {
        return NEU_ERR_PLUGIN_NAME_TOO_LONG;
    }

    cmd->n_group = req->groups.len;
    cmd->groups  = calloc(cmd->n_group, sizeof(*cmd->groups));
    if (NULL == cmd->groups) {
        return NEU_ERR_EINTERNAL;
    }

    for (int i = 0; i < req->groups.len; i++) {
        neu_json_template_group_t *   req_grp = &req->groups.groups[i];
        neu_reqresp_template_group_t *cmd_grp = &cmd->groups[i];

        if (strlen(req_grp->name) >= NEU_GROUP_NAME_LEN) {
            ret = NEU_ERR_GROUP_NAME_TOO_LONG;
            goto error;
        }

        if (req_grp->interval < NEU_GROUP_INTERVAL_LIMIT) {
            ret = NEU_ERR_GROUP_PARAMETER_INVALID;
            goto error;
        }

        cmd_grp->n_tag = req_grp->tags.len;
        cmd_grp->tags  = calloc(cmd_grp->n_tag, sizeof(*cmd_grp->tags));
        if (NULL == cmd_grp->tags) {
            ret = NEU_ERR_EINTERNAL;
            goto error;
        }

        strncpy(cmd_grp->name, req_grp->name, sizeof(cmd_grp->name));
        cmd_grp->interval = req_grp->interval;

        for (int j = 0; j < req_grp->tags.len; ++j) {
            neu_datatag_t * tag     = &cmd_grp->tags[j];
            neu_json_tag_t *req_tag = &req_grp->tags.tags[j];
            if (0 != (ret = set_tag_by_json(tag, req_tag))) {
                goto error;
            }
        }
    }

    strncpy(cmd->name, req->name, sizeof(cmd->name));
    strncpy(cmd->plugin, req->plugin, sizeof(cmd->plugin));

    for (int i = 0; i < req->groups.len; ++i) {
        for (int j = 0; j < req->groups.groups[i].tags.len; ++j) {
            // ownership moved
            req->groups.groups[i].tags.tags[j].name        = NULL;
            req->groups.groups[i].tags.tags[j].address     = NULL;
            req->groups.groups[i].tags.tags[j].description = NULL;
        }
    }

    return ret;

error:
    for (int i = 0; i < cmd->n_group && cmd->groups[i].tags; ++i) {
        free(cmd->groups[i].tags);
    }
    free(cmd->groups);
    return ret;
}

void handle_add_template(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_template_t, neu_json_decode_template, {
            int ret = 0;

            neu_reqresp_head_t header = { 0 };
            header.type               = NEU_REQ_ADD_TEMPLATE;
            header.ctx                = aio;

            neu_req_add_template_t cmd = { 0 };
            ret                        = move_template_json(&cmd, req);
            if (0 == ret) {
                if (0 != neu_plugin_op(plugin, header, &cmd)) {
                    ret = NEU_ERR_IS_BUSY;
                }
            }

            if (0 != ret) {
                NEU_JSON_RESPONSE_ERROR(
                    ret, { neu_http_response(aio, ret, result_error); });
            }
        })
}

void handle_del_template(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    NEU_VALIDATE_JWT(aio);

    neu_reqresp_head_t header = {
        .ctx  = aio,
        .type = NEU_REQ_DEL_TEMPLATE,
    };

    neu_req_del_template_t cmd = { 0 };

    neu_http_get_param_str(aio, "name", cmd.name, sizeof(cmd.name));

    if (0 != neu_plugin_op(plugin, header, &cmd)) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
            neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
        });
    }
}
