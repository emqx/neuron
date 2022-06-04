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

#ifndef _MESSAGE_H_
#define _MESSAGE_H_

#include <assert.h>

#include <nng/nng.h>

#include "adapter.h"
#include "define.h"

typedef enum msg_type {
    MSG_GROUP_READ,
    MSG_GROUP_READ_RESP,
    MSG_TAG_WRITE,
    MSG_TAG_WRITE_RESP,
    MSG_TRANS_DATA,

    MSG_NODE_INIT,
    MSG_PERSISTENCE_LOAD,
    MSG_PLUGIN_ADD,
    MSG_PLUGIN_DEL,
    MSG_PLUGIN_GET,
    MSG_PLUGIN_GET_RESP,
    MSG_NODE_ADD,
    MSG_NODE_DEL,
    MSG_NDOE_GET,
    MSG_NODE_GET_RESP,
    MSG_NODE_SETTING,
    MSG_GROUP_ADD,
    MSG_GROUP_DEL,
    MSG_GROUP_UPDATE,
    MSG_GROUP_GET,
    MSG_GROUP_GET_RESP,
    MSG_TAG_ADD,
    MSG_TAG_ADD_RESP,
    MSG_TAG_DEL,
    MSG_TAG_UPDATE,
    MSG_TAG_UPDATE_RESP,
    MSG_TAG_GET,
    MSG_TAG_GET_RESP,
    MSG_GROUP_SUBSCRIBE,
    MSG_GROUP_UNSUBSCRIBE,
    MSG_UPDATE_LICENSE,

    MSG_ERROR,
} msg_type_e;

typedef struct msg_header {
    msg_type_e type;
    uint16_t   len;
} msg_header_t;

typedef struct msg_error {
    int error;
} msg_error_t;

typedef struct msg_node_init {
    msg_header_t header;
    char         node[NEU_NODE_NAME_LEN];
} msg_node_init_t;

typedef struct msg_license_update {
    msg_header_t header;
} msg_license_update_t;

typedef struct msg_group_read {
    msg_header_t header;
    char         driver[NEU_NODE_NAME_LEN];
    char         reader[NEU_NODE_NAME_LEN];
    char         group[NEU_GROUP_NAME_LEN];
    void *       ctx;
} msg_group_read_t;

typedef struct msg_trans_data {
    msg_header_t       header;
    neu_reqresp_data_t data;
} msg_trans_data_t;

typedef struct msg_group_read_resp {
    msg_header_t            header;
    char                    reader[NEU_NODE_NAME_LEN];
    neu_reqresp_read_resp_t data;
} msg_group_read_resp_t;

typedef struct msg_tag_write {
    msg_header_t header;
    char         writer[NEU_NODE_NAME_LEN];
    char         driver[NEU_NODE_NAME_LEN];
    char         group[NEU_GROUP_NAME_LEN];
    char         tag[NEU_TAG_NAME_LEN];
    neu_dvalue_t value;
    void *       ctx;
} msg_tag_write_t;

typedef struct msg_tag_write_resp {
    msg_header_t header;
    char         writer[NEU_NODE_NAME_LEN];
    void *       ctx;
    int          error;
} msg_tag_write_resp_t;

typedef struct msg_persistence_load {
    msg_header_t header;
} msg_persistence_load_t;

typedef struct msg_add_plugin {
    msg_header_t header;
    char         library[NEU_PLUGIN_LIBRARY_LEN];
} msg_add_plugin_t;

typedef struct msg_del_plugin {
    msg_header_t header;
    char         plugin[NEU_PLUGIN_NAME_LEN];
} msg_del_plugin_t;

typedef struct msg_get_plugin {
    msg_header_t header;
} msg_get_plugin_t;

typedef struct msg_add_node {
    msg_header_t header;
    char         node[NEU_NODE_NAME_LEN];
    char         plugin[NEU_PLUGIN_NAME_LEN];
} msg_add_node_t;

typedef struct msg_del_node {
    msg_header_t header;
    char         node[NEU_NODE_NAME_LEN];
} msg_del_node_t;

typedef struct msg_get_ndoe {
    msg_header_t    header;
    neu_node_type_e type;
} msg_get_node_t;

typedef struct msg_add_group {
    msg_header_t header;
    char         driver[NEU_NODE_NAME_LEN];
    char         group[NEU_GROUP_NAME_LEN];
    uint32_t     interval;
} msg_add_group_t;

typedef struct msg_del_group {
    msg_header_t header;
    char         driver[NEU_NODE_NAME_LEN];
    char         group[NEU_GROUP_NAME_LEN];
} msg_del_group_t;

typedef struct msg_update_group {
    msg_header_t header;
    char         driver[NEU_NODE_NAME_LEN];
    char         group[NEU_GROUP_NAME_LEN];
    uint32_t     interval;
} msg_update_group_t;

typedef struct msg_get_group {
    msg_header_t header;
    char         driver[NEU_NODE_NAME_LEN];
} msg_get_group_t;

typedef struct {
    msg_header_t  header;
    char          driver[NEU_NODE_NAME_LEN];
    char          group[NEU_GROUP_NAME_LEN];
    uint16_t      n_tag;
    neu_datatag_t tags[];
} msg_add_tag_t, msg_update_tag_t;

typedef struct msg_del_tag {
    msg_header_t header;
    char         driver[NEU_NODE_NAME_LEN];
    char         group[NEU_GROUP_NAME_LEN];
    uint16_t     n_tag;
    char *       tags[];
} msg_del_tag_t;

typedef struct {
    msg_header_t header;
    char         app[NEU_NODE_NAME_LEN];
    char         driver[NEU_NODE_NAME_LEN];
    char         group[NEU_GROUP_NAME_LEN];
} msg_group_subscribe_t, msg_group_unsubscribe_t;

typedef struct msg_node_setting {
    msg_header_t header;
    char         node[NEU_NODE_NAME_LEN];
    char *       setting;
} msg_node_setting_t;

inline static void *msg_new(nng_msg **msg, msg_type_e type, uint16_t size)
{
    nng_msg_alloc(msg, size);
    msg_header_t *header = (msg_header_t *) nng_msg_body(*msg);

    header->type = type;
    header->len  = size;

    return nng_msg_body(*msg);
}

#endif
