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
#include <string.h>

#include "neu_log.h"
#include "utils/json.h"

#include "neu_json_group_config.h"
#include "neu_json_login.h"
#include "neu_json_node.h"
#include "neu_json_parser.h"
#include "neu_json_plugin.h"
#include "neu_json_rw.h"
#include "neu_json_tag.h"

int neu_parse_decode(char *buf, void **result)
{
    neu_json_elem_t func_code = {
        .name = "function",
        .t    = NEU_JSON_INT,
    };
    int ret = neu_json_decode(buf, 1, &func_code);
    if (ret != 0) {
        return -1;
    }

    switch (func_code.v.val_int) {
    case NEU_PARSE_OP_GET_GROUP_CONFIG: {
        ret = neu_paser_decode_get_group_config_req(
            buf, (struct neu_paser_get_group_config_req **) result);
        break;
    }
    case NEU_PARSE_OP_ADD_GROUP_CONFIG: {
        ret = neu_paser_decode_add_group_config_req(
            buf, (struct neu_paser_add_group_config_req **) result);
        break;
    }
    case NEU_PARSE_OP_UPDATE_GROUP_CONFIG: {
        ret = neu_paser_decode_update_group_config_req(
            buf, (struct neu_paser_update_group_config_req **) result);
        break;
    }
    case NEU_PARSE_OP_DELETE_GROUP_CONFIG: {
        ret = neu_paser_decode_delete_group_config_req(
            buf, (struct neu_paser_delete_group_config_req **) result);
        break;
    }
    case NEU_PARSE_OP_READ:
        ret = neu_parse_decode_read_req(buf,
                                        (struct neu_parse_read_req **) result);
        break;
    case NEU_PARSE_OP_WRITE:
        ret = neu_parse_decode_write_req(
            buf, (struct neu_parse_write_req **) result);
        break;
    case NEU_PARSE_OP_LOGIN:
        ret = neu_parse_decode_login_req(
            buf, (struct neu_parse_login_req **) result);
        break;
    case NEU_PARSE_OP_LOGOUT:
        break;
    case NEU_PARSE_OP_ADD_TAGS:
        ret = neu_parse_decode_add_tags_req(
            buf, (struct neu_parse_add_tags_req **) result);
        break;
    case NEU_PARSE_OP_GET_TAGS:
        ret = neu_parse_decode_get_tags_req(
            buf, (struct neu_parse_get_tags_req **) result);
        break;
    case NEU_PARSE_OP_DELETE_TAGS:
        ret = neu_parse_decode_delete_tags_req(
            buf, (struct neu_parse_delete_tags_req **) result);
        break;
    case NEU_PARSE_OP_UPDATE_TAGS:
        ret = neu_parse_decode_update_tags_req(
            buf, (struct neu_parse_update_tags_req **) result);
        break;
    case NEU_PARSE_OP_ADD_NODES:
        ret = neu_parse_decode_add_nodes_req(
            buf, (struct neu_parse_add_nodes_req **) result);
        break;
    case NEU_PARSE_OP_GET_NODES:
        ret = neu_parse_decode_get_nodes_req(
            buf, (struct neu_parse_get_nodes_req **) result);
        break;
    case NEU_PARSE_OP_DELETE_NODES:
        ret = neu_parse_decode_delete_nodes_req(
            buf, (struct neu_parse_delete_nodes_req **) result);
        break;
    case NEU_PARSE_OP_UPDATE_NODES:
        ret = neu_parse_decode_update_nodes_req(
            buf, (struct neu_parse_update_nodes_req **) result);
        break;
    case NEU_PARSE_OP_ADD_PLUGIN:
        ret = neu_parse_decode_add_plugin_req(
            buf, (struct neu_parse_add_plugin_req **) result);
        break;
    case NEU_PARSE_OP_DELETE_PLUGIN:
        ret = neu_parse_decode_delete_plugin_req(
            buf, (struct neu_parse_delete_plugin_req **) result);
        break;
    case NEU_PARSE_OP_UPDATE_PLUGIN:
        ret = neu_parse_decode_update_plugin_req(
            buf, (struct neu_parse_update_plugin_req **) result);
        break;
    case NEU_PARSE_OP_GET_PLUGIN:
        ret = neu_parse_decode_get_plugin_req(
            buf, (struct neu_parse_get_plugin_req **) result);
        break;
    }

    return ret;
}

int neu_parse_encode(void *result, char **buf)
{
    enum neu_parse_function *function = result;

    switch (*function) {
    case NEU_PARSE_OP_GET_GROUP_CONFIG: {
        neu_parse_encode_get_group_config_res(
            (struct neu_paser_get_group_config_res *) result, buf);
        break;
    }
    case NEU_PARSE_OP_ADD_GROUP_CONFIG: {
        neu_parse_encode_add_group_config_res(
            (struct neu_paser_group_config_res *) result, buf);
        break;
    }
    case NEU_PARSE_OP_UPDATE_GROUP_CONFIG: {
        neu_parse_encode_update_group_config_res(
            (struct neu_paser_group_config_res *) result, buf);
        break;
    }
    case NEU_PARSE_OP_DELETE_GROUP_CONFIG: {
        neu_parse_encode_delete_group_config_res(
            (struct neu_paser_group_config_res *) result, buf);
        break;
    }
    case NEU_PARSE_OP_READ:
        neu_parse_encode_read_res((struct neu_parse_read_res *) result, buf);
        break;
    case NEU_PARSE_OP_WRITE:
        neu_parse_encode_write_res((struct neu_parse_write_res *) result, buf);
        break;
    case NEU_PARSE_OP_LOGIN:
        neu_parse_encode_login_res((struct neu_parse_login_res *) result, buf);
        break;
    case NEU_PARSE_OP_LOGOUT:
        break;
    case NEU_PARSE_OP_ADD_TAGS:
        neu_parse_encode_add_tags_res((struct neu_parse_add_tags_res *) result,
                                      buf);
        break;
    case NEU_PARSE_OP_GET_TAGS:
        neu_parse_encode_get_tags_res((struct neu_parse_get_tags_res *) result,
                                      buf);
        break;
    case NEU_PARSE_OP_DELETE_TAGS:
        neu_parse_encode_delete_tags_res(
            (struct neu_parse_delete_tags_res *) result, buf);
        break;
    case NEU_PARSE_OP_UPDATE_TAGS:
        neu_parse_encode_update_tags_res(
            (struct neu_parse_update_tags_res *) result, buf);
        break;
    case NEU_PARSE_OP_ADD_NODES:
        neu_parse_encode_add_nodes_res(
            (struct neu_parse_add_nodes_res *) result, buf);
        break;
    case NEU_PARSE_OP_GET_NODES:
        neu_parse_encode_get_nodes_res(
            (struct neu_parse_get_nodes_res *) result, buf);
        break;
    case NEU_PARSE_OP_DELETE_NODES:
        neu_parse_encode_delete_nodes_res(
            (struct neu_parse_delete_nodes_res *) result, buf);
        break;
    case NEU_PARSE_OP_UPDATE_NODES:
        neu_parse_encode_update_nodes_res(
            (struct neu_parse_update_nodes_res *) result, buf);
        break;
    case NEU_PARSE_OP_ADD_PLUGIN:
        neu_parse_encode_add_plugin_res(
            (struct neu_parse_add_plugin_res *) result, buf);
        break;
    case NEU_PARSE_OP_DELETE_PLUGIN:
        neu_parse_encode_delete_plugin_res(
            (struct neu_parse_delete_plugin_res *) result, buf);
        break;
    case NEU_PARSE_OP_UPDATE_PLUGIN:
        neu_parse_encode_update_plugin_res(
            (struct neu_parese_update_plugin_res *) result, buf);
        break;
    case NEU_PARSE_OP_GET_PLUGIN:
        neu_parse_encode_get_plugin_res(
            (struct neu_parse_get_plugin_res *) result, buf);
        break;
    }

    return 0;
}

void neu_parse_decode_free(void *result)
{
    enum neu_parse_function *function = result;

    switch (*function) {
    case NEU_PARSE_OP_GET_GROUP_CONFIG: {
        struct neu_paser_get_group_config_req *req = result;

        neu_parse_encode_get_group_config_free(req);
        break;
    }
    case NEU_PARSE_OP_ADD_GROUP_CONFIG: {
        struct neu_paser_add_group_config_req *req = result;

        neu_paser_decode_add_group_config_free(req);
        break;
    }
    case NEU_PARSE_OP_UPDATE_GROUP_CONFIG: {
        struct neu_paser_update_group_config_req *req = result;

        neu_paser_decode_update_group_config_free(req);
        break;
    }
    case NEU_PARSE_OP_DELETE_GROUP_CONFIG: {
        struct neu_paser_delete_group_config_req *req = result;

        neu_paser_decode_delete_group_config_free(req);
        break;
    }
    case NEU_PARSE_OP_READ: {
        struct neu_parse_read_req *req = result;

        neu_parse_decode_read_free(req);
        break;
    }
    case NEU_PARSE_OP_WRITE: {
        struct neu_parse_write_req *req = result;

        neu_parse_decode_write_free(req);
        break;
    }

    case NEU_PARSE_OP_LOGIN: {
        struct neu_parse_login_req *req = result;

        free(req->uuid);
        free(req->user);
        free(req->password);
        break;
    }
    case NEU_PARSE_OP_LOGOUT: {
        break;
    }
    case NEU_PARSE_OP_ADD_TAGS: {
        struct neu_parse_add_tags_req *req = result;

        neu_parse_decode_add_tags_free(req);
        break;
    }
    case NEU_PARSE_OP_GET_TAGS: {
        struct neu_parse_get_tags_req *req = result;

        neu_parse_decode_get_tags_free(req);
        break;
    }
    case NEU_PARSE_OP_DELETE_TAGS: {
        struct neu_parse_delete_tags_req *req = result;

        neu_parse_decode_delete_tags_free(req);
        break;
    }
    case NEU_PARSE_OP_UPDATE_TAGS: {
        struct neu_parse_update_tags_req *req = result;

        neu_parse_decode_update_tags_free(req);
        break;
    }

    case NEU_PARSE_OP_ADD_NODES: {
        struct neu_parse_add_nodes_req *req = result;

        free(req->uuid);
        free(req->adapter_name);
        free(req->plugin_name);
        break;
    }
    case NEU_PARSE_OP_GET_NODES: {
        struct neu_parse_get_nodes_req *req = result;

        free(req->uuid);
        break;
    }
    case NEU_PARSE_OP_DELETE_NODES: {
        struct neu_parse_delete_nodes_req *req = result;

        free(req->uuid);
        break;
    }
    case NEU_PARSE_OP_UPDATE_NODES: {
        struct neu_parse_update_nodes_req *req = result;

        free(req->adapter_name);
        free(req->plugin_name);
        free(req->uuid);

        break;
    }
    case NEU_PARSE_OP_ADD_PLUGIN: {
        struct neu_parse_add_plugin_req *req = result;

        neu_parse_decode_add_plugin_free(req);
        break;
    }
    case NEU_PARSE_OP_DELETE_PLUGIN: {
        struct neu_parse_delete_plugin_req *req = result;

        neu_parse_decode_delete_plugin_free(req);
        break;
    }
    case NEU_PARSE_OP_UPDATE_PLUGIN: {
        struct neu_parse_update_plugin_req *req = result;
        neu_parse_decode_update_plugin_free(req);
        break;
    }
    case NEU_PARSE_OP_GET_PLUGIN: {
        struct neu_parse_get_plugin_req *req = result;

        neu_parse_decode_get_plugin_free(req);
        break;
    }
    }

    free(result);
}