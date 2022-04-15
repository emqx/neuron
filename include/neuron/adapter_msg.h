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

#ifndef _NEU_ADAPTER_MSG_H_
#define _NEU_ADAPTER_MSG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <string.h>
#include <unistd.h>

//#include "adapter.h"
#include "data_expr.h"
#include "datatag_table.h"
#include "errcodes.h"
#include "plugin_info.h"
#include "tag_group_config.h"
#include "types.h"

#define DEFAULT_TAG_GROUP_COUNT 8

/**
 * definition enum and structure for neuron request and response
 */

typedef enum neu_reqresp_type {
    NEU_REQRESP_NOP = 0,
    NEU_REQRESP_ERR_CODE, // result code of command
    NEU_REQRESP_READ_DATA,
    NEU_REQRESP_READ_RESP,
    NEU_REQRESP_WRITE_DATA,
    NEU_REQRESP_WRITE_RESP,
    NEU_REQRESP_SUBSCRIBE_NODE,
    NEU_REQRESP_UNSUBSCRIBE_NODE,
    NEU_REQRESP_TRANS_DATA,
    NEU_REQRESP_ADD_NODE,
    NEU_REQRESP_DEL_NODE,
    NEU_REQRESP_GET_NODES,
    NEU_REQRESP_NODES,
    NEU_REQRESP_ADD_GRP_CONFIG,
    NEU_REQRESP_DEL_GRP_CONFIG,
    NEU_REQRESP_UPDATE_GRP_CONFIG,
    NEU_REQRESP_GET_GRP_CONFIGS,
    NEU_REQRESP_GRP_CONFIGS,
    NEU_REQRESP_ADD_PLUGIN_LIB,
    NEU_REQRESP_DEL_PLUGIN_LIB,
    NEU_REQRESP_UPDATE_PLUGIN_LIB,
    NEU_REQRESP_GET_PLUGIN_LIBS,
    NEU_REQRESP_PLUGIN_LIBS,
    NEU_REQRESP_GET_DATATAGS,
    NEU_REQRESP_DATATAGS,
    NEU_REQRESP_SELF_NODE_ID,
    NEU_REQRESP_NODE_ID,
    NEU_REQRESP_SELF_NODE_NAME,
    NEU_REQRESP_NODE_NAME,
    NEU_REQRESP_GET_NODE_ID_BY_NAME,
    NEU_REQRESP_SET_NODE_SETTING,
    NEU_REQRESP_GET_NODE_SETTING,
    NEU_REQRESP_GET_NODE_SETTING_RESP,
    NEU_REQRESP_GET_NODE_STATE,
    NEU_REQRESP_NODE_STATE,
    NEU_REQRESP_NODE_CTL,
    NEU_REQRESP_GET_SUB_GRP_CONFIGS,
    NEU_REQRESP_SUB_GRP_CONFIGS_RESP,
    NEU_REQRESP_VALIDATE_TAG,
    NEU_REQRESP_UPDATE_LICENSE,
} neu_reqresp_type_e;

/* NEU_REQRESP_READ_DATA */
typedef struct neu_reqresp_read {
    neu_taggrp_config_t *grp_config;
    neu_node_id_t        dst_node_id;
} neu_reqresp_read_t;

/* NEU_REQRESP_READ_RESP */
typedef struct neu_reqresp_read_resp {
    neu_taggrp_config_t *grp_config;
    neu_data_val_t *     data_val; ///< data values of reading
} neu_reqresp_read_resp_t;

/* NEU_REQRESP_WRITE_DATA */
typedef struct neu_reqresp_write {
    neu_taggrp_config_t *grp_config;
    neu_node_id_t        dst_node_id;
    neu_data_val_t *     data_val;
} neu_reqresp_write_t;

/* NEU_REQRESP_WRITE_RESP */
typedef struct neu_reqresp_write_resp {
    neu_taggrp_config_t *grp_config;
    neu_data_val_t *     data_val; ///< result status of writing
} neu_reqresp_write_resp_t;

/* NEU_REQRESP_SUBSCRIBE_NODE */
typedef struct neu_reqresp_subscribe_node {
    neu_taggrp_config_t *grp_config;
    neu_node_id_t        dst_node_id;
    neu_node_id_t        src_node_id;
} neu_reqresp_subscribe_node_t;

/* NEU_REQRESP_UNSUBSCRIBE_NODE */
typedef struct neu_reqresp_unsubscribe_node {
    neu_taggrp_config_t *grp_config;
    neu_node_id_t        dst_node_id;
    neu_node_id_t        src_node_id;
} neu_reqresp_unsubscribe_node_t;

/* NEU_REQRESP_TRANS_DATA */
typedef struct neu_reqresp_data {
    neu_taggrp_config_t *grp_config;
    neu_data_val_t *     data_val;
} neu_reqresp_data_t;

typedef struct neu_node_info {
    neu_node_id_t node_id;
    char *        node_name;
    plugin_id_t   plugin_id;
    // TODO: add node attribute
} neu_node_info_t;

/* NEU_REQRESP_ADD_NODE */
typedef struct neu_cmd_add_node {
    neu_node_type_e node_type;
    const char *    adapter_name;
    const char *    plugin_name;
    // optional value, it is nothing when set to 0
    plugin_id_t plugin_id;
} neu_cmd_add_node_t;

/* NEU_REQRESP_DEL_NODE */
typedef struct neu_cmd_del_node {
    neu_node_id_t node_id;
} neu_cmd_del_node_t;

/* NEU_REQRESP_GET_NODES */
typedef struct neu_cmd_get_nodes {
    neu_node_type_e node_type;
} neu_cmd_get_nodes_t;

/* NEU_REQRESP_NODES */
typedef struct neu_reqresp_nodes {
    vector_t nodes; // vector of neu_node_info_t
} neu_reqresp_nodes_t;

/* NEU_REQRESP_ADD_GRP_CONFIG */
typedef struct neu_cmd_add_grp_config {
    neu_node_id_t        node_id;
    neu_taggrp_config_t *grp_config;
} neu_cmd_add_grp_config_t;

/* NEU_REQRESP_DEL_GRP_CONFIG */
typedef struct neu_cmd_del_grp_config {
    neu_node_id_t node_id;
    char *        config_name;
} neu_cmd_del_grp_config_t;

/* NEU_REQRESP_UPDATE_GRP_CONFIG */
typedef struct neu_cmd_update_grp_config {
    neu_node_id_t        node_id;
    neu_taggrp_config_t *grp_config;
} neu_cmd_update_grp_config_t;

/* NEU_REQRESP_GET_GRP_CONFIGS */
typedef struct neu_cmd_get_grp_configs {
    neu_node_id_t node_id; // get group configs of this node
} neu_cmd_get_grp_configs_t;

/* NEU_REQRESP_GRP_CONFIGS */
typedef struct neu_reqresp_grp_configs {
    vector_t grp_configs; // vector of neu_taggrp_config_t pointer
} neu_reqresp_grp_configs_t;

typedef struct plugin_lib_info {
    plugin_id_t     plugin_id;
    plugin_kind_e   plugin_kind;
    neu_node_type_e node_type;
    // The buffer is reference from plugin entity of register table,
    // don't free it
    const char *plugin_name;
    // The buffer is reference from plugin entity of register table,
    // don't free it
    const char *plugin_lib_name;
} plugin_lib_info_t;

/* NEU_REQRESP_ADD_PLUGIN_LIB */
typedef struct neu_cmd_add_plugin_lib_t {
    const char *plugin_lib_name;
} neu_cmd_add_plugin_lib_t;

/* NEU_REQRESP_DEL_PLUGIN_LIB */
typedef struct neu_cmd_del_plugin_lib {
    plugin_id_t plugin_id;
} neu_cmd_del_plugin_lib_t;

/* NEU_REQRESP_UPDATE_PLUGIN_LIB */
typedef struct neu_cmd_update_plugin_lib {
    plugin_kind_e   plugin_kind;
    neu_node_type_e node_type;
    const char *    plugin_name;
    const char *    plugin_lib_name;
} neu_cmd_update_plugin_lib_t;

/* NEU_REQRESP_GET_PLUGIN_LIBS */
typedef struct neu_cmd_get_plugin_libs {
    uint32_t reserved;
} neu_cmd_get_plugin_libs_t;

/* NEU_REQRESP_PLUGIN_LIBS */
typedef struct neu_reqresp_plugin_libs {
    vector_t plugin_libs; // vector of plugin_lib_info_t
} neu_reqresp_plugin_libs_t;

/* NEU_REQRESP_GET_DATATAGS */
typedef struct neu_cmd_get_datatags {
    neu_node_id_t node_id; // get datatag table of this node
} neu_cmd_get_datatags_t;

/* NEU_REQRESP_DATATAGS */
typedef struct neu_reqresp_datatags {
    neu_datatag_table_t *datatag_tbl; // datatag table of a node
} neu_reqresp_datatags_t;

/* NEU_REQRESP_SELF_NODE_ID */
typedef struct neu_cmd_self_node_id {
    uint32_t reserved;
} neu_cmd_self_node_id_t;

/* NEU_REQRESP_GET_NODE_ID_BY_NAME */
typedef struct neu_cmd_get_node_id_by_name {
    const char *name;
} neu_cmd_get_node_id_by_name_t;

/* NEU_REQRESP_NODE_ID */
typedef struct neu_represp_node_id {
    neu_node_id_t node_id;
} neu_reqresp_node_id_t;

/* NEU_REQRESP_SELF_NODE_NAME */
typedef struct neu_cmd_self_node_name {
    uint32_t reserved;
} neu_cmd_self_node_name_t;

/* NEU_REQRESP_NODE_NAME */
typedef struct neu_represp_node_name {
    const char *node_name;
} neu_reqresp_node_name_t;

/* NEU_REQRESP_SET_NODE_SETTING */
typedef struct neu_cmd_set_node_setting {
    neu_node_id_t node_id;
    const char *  setting;
} neu_cmd_set_node_setting_t;

/* NEU_REQRESP_GET_NODE_SETTING */
typedef struct neu_cmd_get_node_setting {
    neu_node_id_t node_id;
} neu_cmd_get_node_setting_t;

typedef struct neu_reqresp_node_setting {
    int32_t result;
    char *  setting;
} neu_reqresp_node_setting_t;

/* NEU_REQRESP_GET_NODE_STATE */
typedef struct neu_cmd_get_node_state {
    neu_node_id_t node_id;
} neu_cmd_get_node_state_t;

/* NEU_REQRESP_NODE_STATE */
typedef struct neu_reqresp_node_state {
    int32_t            result;
    neu_plugin_state_t state;
} neu_reqresp_node_state_t;

/* NEU_REQRESP_GET_SUB_GRP_CONFIGS */
typedef struct neu_cmd_get_sub_grp_configs {
    neu_node_id_t node_id;
} neu_cmd_get_sub_grp_configs_t;

/* NEU_REQRESP_SUB_GRP_CONFIGS_RESP */
typedef struct neu_reqresp_sub_grp_configs {
    int32_t   result;
    vector_t *sub_grp_configs;
} neu_reqresp_sub_grp_configs_t;

typedef enum neu_adapter_ctl {
    NEU_ADAPTER_CTL_START = 0,
    NEU_ADAPTER_CTL_STOP,
} neu_adapter_ctl_e;

/* NEU_REQRESP_NODE_CTL */
typedef struct neu_cmd_node_ctl {
    neu_node_id_t     node_id;
    neu_adapter_ctl_e ctl;
} neu_cmd_node_ctl_t;

/* NEU_REQRESP_VALIDATE_TAG */
typedef struct neu_cmd_validate_tag {
    neu_node_id_t  node_id;
    neu_datatag_t *tag;
} neu_cmd_validate_tag_t;

/**
 * definition enum and structure for neuron event
 */
typedef enum neu_event_type {
    NEU_EVENT_NOP,
    NEU_EVENT_STATUS,
    NEU_EVENT_ADD_TAGS,
    NEU_EVENT_UPDATE_TAGS,
    NEU_EVENT_DEL_TAGS,
    NEU_EVENT_UPDATE_LICENSE,
} neu_event_type_e;

typedef struct neu_event_notify {
    uint32_t         event_id;
    neu_event_type_e type;
    uint32_t         buf_len;
    void *           buf;
} neu_event_notify_t;

typedef struct neu_event_reply {
    uint32_t         event_id;
    neu_event_type_e type;
    uint32_t         buf_len;
    void *           buf;
} neu_event_reply_t;

typedef struct neu_event_tags {
    neu_node_id_t node_id;
    char *        grp_config_name;
} neu_event_tags_t;

typedef struct {
    char *src_name;
    char *dst_name;
} neu_event_update_node_t;

typedef struct neu_request {
    uint32_t           req_id;
    neu_reqresp_type_e req_type;
    uint64_t           sender_id; // adapter_id of sender
    char *             node_name; // adapter name of sender
    uint32_t           buf_len;
    void *             buf;
} neu_request_t;

typedef struct neu_response {
    uint32_t           req_id;
    neu_reqresp_type_e resp_type;
    uint64_t           recver_id; // adapter_id of reciever, it is same as
                                  // sender_id in neu_request_t
    uint32_t buf_len;
    void *   buf;
} neu_response_t;
#ifdef __cplusplus
}
#endif

#endif
