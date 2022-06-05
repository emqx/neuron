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

#include "define.h"
#include "errcodes.h"
#include "plugin_info.h"
#include "tag.h"
#include "utils/utextend.h"

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
    NEU_REQRESP_GET_NODE_BY_ID,
    NEU_REQRESP_NODE_INFO,
    NEU_REQRESP_GET_NODES,
    NEU_REQRESP_NODES,
    NEU_REQRESP_ADD_GRP_CONFIG,
    NEU_REQRESP_DEL_GRP_CONFIG,
    NEU_REQRESP_UPDATE_GRP_CONFIG,
    NEU_REQRESP_GET_GRP_CONFIGS,
    NEU_REQRESP_ADD_TAGS,
    NEU_REQRESP_DEL_TAGS,
    NEU_REQRESP_UPDATE_TAGS,
    NEU_REQRESP_GET_TAGS,
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

typedef struct neu_reqresp_head {
    uint32_t           req_id;
    neu_reqresp_type_e type;
    uint16_t           len;
} neu_reqresp_head_t;

typedef struct neu_plugin_lib_info {
    char name[NEU_PLUGIN_NAME_LEN];
    char description[NEU_PLUGIN_DESCRIPTION_LEN];
    char lib_name[NEU_PLUGIN_LIBRARY_LEN];

    neu_plugin_kind_e kind;
    neu_node_type_e   type;
} neu_plugin_lib_info_t;

typedef struct neu_node_info {
    char node_name[NEU_NODE_NAME_LEN];
    char plugin_name[NEU_PLUGIN_NAME_LEN];
} neu_node_info_t;

typedef struct neu_group_info {
    char     name[NEU_GROUP_NAME_LEN];
    uint16_t tag_count;
    uint32_t interval;
} neu_group_info_t;

typedef struct neu_subscribe_info {
    char app[NEU_NODE_NAME_LEN];
    char driver[NEU_NODE_NAME_LEN];
    char group[NEU_GROUP_NAME_LEN];
} neu_subscribe_info_t;

typedef struct {
    const char *   node;
    const char *   group;
    uint16_t       n_tag;
    neu_datatag_t *tags;
} neu_reqresp_add_tag_t, neu_reqresp_update_tag_t;

typedef struct neu_reqresp_del_tag {
    const char *node;
    const char *group;
    uint16_t    n_tag;
    char **     tags;
} neu_reqresp_del_tag_t;

typedef struct {
    int      error;
    uint16_t index;
} neu_reqresp_add_tag_resp_t, neu_reqresp_update_tag_resp_t,
    neu_reqresp_del_tag_resp_t;

typedef struct neu_reqresp_get_tags {
    const char *node;
    const char *group;
} neu_reqresp_get_tags_t;

typedef struct neu_reqresp_get_tags_resp {
    int       error;
    UT_array *tags;
} neu_reqresp_get_tags_resp_t;

typedef struct neu_tag_data {
    char         tag[NEU_TAG_NAME_LEN];
    neu_dvalue_t value;
} neu_tag_data_t;

/* NEU_REQRESP_DATA */
typedef struct neu_reqresp_data {
    char           driver[NEU_NODE_NAME_LEN];
    char           group[NEU_GROUP_NAME_LEN];
    uint16_t       n_data;
    neu_tag_data_t datas[];
} neu_reqresp_data_t;

/* NEU_REQRESP_READ_DATA */
typedef struct neu_reqresp_read {
    char  driver[NEU_NODE_NAME_LEN];
    char  group[NEU_GROUP_NAME_LEN];
    void *ctx;
} neu_reqresp_read_t;

/* NEU_REQRESP_READ_RESP */
typedef struct neu_reqresp_read_resp {
    char           driver[NEU_NODE_NAME_LEN];
    char           group[NEU_GROUP_NAME_LEN];
    void *         ctx;
    uint16_t       n_data;
    neu_tag_data_t datas[];
} neu_reqresp_read_resp_t;

/* NEU_REQRESP_WRITE_DATA */
typedef struct neu_reqresp_write {
    char         driver[NEU_NODE_NAME_LEN];
    char         group[NEU_GROUP_NAME_LEN];
    char         tag[NEU_TAG_NAME_LEN];
    void *       ctx;
    neu_dvalue_t value;
} neu_reqresp_write_t;

typedef struct neu_reqresp_write_resp {
    void *ctx;
    int   error;
} neu_reqresp_write_resp_t;

/* NEU_REQRESP_WRITE_RESP */

/* NEU_REQRESP_UNSUBSCRIBE_NODE */
/* NEU_REQRESP_SUBSCRIBE_NODE */
typedef struct {
    char *app;
    char *driver;
    char *group;
} neu_reqresp_subscribe_node_t, neu_reqresp_unsubscribe_node_t;

/* NEU_REQRESP_TRANS_DATA */

/* NEU_REQRESP_ADD_NODE */
typedef struct neu_cmd_add_node {
    const char *adapter_name;
    const char *plugin_name;
} neu_cmd_add_node_t;

/* NEU_REQRESP_DEL_NODE */
typedef struct neu_cmd_del_node {
    char *name;
} neu_cmd_del_node_t;

/* NEU_REQRESP_GET_NODE_BY_ID */
typedef struct neu_cmd_get_node_by_id {
    neu_node_id_t node_id;
} neu_cmd_get_node_by_id_t;

/* NEU_REQRESP_NODE_INFO */
typedef struct {
    int32_t         result;
    neu_node_info_t node_info;
} neu_reqresp_node_info_t;

/* NEU_REQRESP_GET_NODES */
typedef struct neu_cmd_get_nodes {
    neu_node_type_e node_type;
} neu_cmd_get_nodes_t;

/* NEU_REQRESP_NODES */
typedef struct neu_reqresp_nodes {
    UT_array *nodes; // array of neu_node_info_t
} neu_reqresp_nodes_t;

/* NEU_REQRESP_ADD_GRP_CONFIG */
typedef struct {
    char *   node_name;
    char *   name;
    uint32_t interval;
} neu_cmd_add_grp_config_t, neu_cmd_update_grp_config_t;

/* NEU_REQRESP_DEL_GRP_CONFIG */
typedef struct neu_cmd_del_grp_config {
    char *node_name;
    char *name;
} neu_cmd_del_grp_config_t;

/* NEU_REQRESP_GET_GRP_CONFIGS */
typedef struct neu_cmd_get_grp_configs {
    char *node_name;
} neu_cmd_get_grp_configs_t;

/* NEU_REQRESP_GRP_CONFIGS */
typedef struct neu_reqresp_grp_configs {
    int       error;
    UT_array *groups;
} neu_reqresp_grp_configs_t;

/* NEU_REQRESP_ADD_PLUGIN_LIB */
typedef struct neu_cmd_add_plugin_lib_t {
    const char *plugin_lib_name;
} neu_cmd_add_plugin_lib_t;

/* NEU_REQRESP_DEL_PLUGIN_LIB */
typedef struct neu_cmd_del_plugin_lib {
    char *name;
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
    UT_array *plugin_libs;
} neu_reqresp_plugin_libs_t;

/* NEU_REQRESP_GET_DATATAGS */
typedef struct neu_cmd_get_datatags {
    neu_node_id_t node_id; // get datatag table of this node
} neu_cmd_get_datatags_t;

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
    char *node_name;
    char *setting;
} neu_cmd_set_node_setting_t;

/* NEU_REQRESP_GET_NODE_SETTING */
typedef struct neu_cmd_get_node_setting {
    char *node_name;
} neu_cmd_get_node_setting_t;

typedef struct neu_reqresp_node_setting {
    int32_t result;
    char *  setting;
} neu_reqresp_node_setting_t;

/* NEU_REQRESP_GET_NODE_STATE */
typedef struct neu_cmd_get_node_state {
    char *node_name;
} neu_cmd_get_node_state_t;

/* NEU_REQRESP_NODE_STATE */
typedef struct neu_reqresp_node_state {
    int32_t            result;
    neu_plugin_state_t state;
} neu_reqresp_node_state_t;

/* NEU_REQRESP_GET_SUB_GRP_CONFIGS */
typedef struct neu_cmd_get_sub_grp_configs {
    char *node_name;
} neu_cmd_get_sub_grp_configs_t;

/* NEU_REQRESP_SUB_GRP_CONFIGS_RESP */
typedef struct neu_reqresp_sub_grp_configs {
    UT_array *groups;
} neu_reqresp_sub_grp_configs_t;

typedef enum neu_adapter_ctl {
    NEU_ADAPTER_CTL_START = 0,
    NEU_ADAPTER_CTL_STOP,
} neu_adapter_ctl_e;

/* NEU_REQRESP_NODE_CTL */
typedef struct neu_cmd_node_ctl {
    char *            node_name;
    neu_adapter_ctl_e ctl;
} neu_cmd_node_ctl_t;

/* NEU_REQRESP_VALIDATE_TAG */
typedef struct neu_cmd_validate_tag {
    char *         node_name;
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
    char *node_name;
    char *group_name;
} neu_event_tags_t;

typedef struct {
    char *src_name;
    char *dst_name;
} neu_event_update_node_t;

typedef struct neu_request {
    uint32_t           req_id;
    neu_reqresp_type_e req_type;
    char *             node_name; // adapter name of sender
    void *             ctx;
    uint32_t           buf_len;
    void *             buf;
} neu_request_t;

typedef struct neu_response {
    uint32_t           req_id;
    neu_reqresp_type_e resp_type;
    char *             node_name;
    uint32_t           buf_len;
    void *             buf;
} neu_response_t;

#ifdef __cplusplus
}
#endif

#endif
