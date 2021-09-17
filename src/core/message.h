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

#include "databuf.h"
#include "neu_log.h"

#include "adapter/adapter_internal.h"
#include "neu_datatag_manager.h"

typedef enum msg_type {
    MSG_TYPE_CMD_START = 0,
    MSG_CMD_NOP        = MSG_TYPE_CMD_START,
    MSG_CMD_READ_DATA,
    MSG_CMD_WRITE_DATA,
    MSG_CMD_START_PERIODIC_READ,
    MSG_CMD_STOP_PERIODIC_READ,
    MSG_CMD_SUBSCRIBE_NODE,
    MSG_CMD_UNSUBSCRIBE_NODE,
    MSG_CMD_GROUP_CONFIG_CHANGED,
    MSG_CMD_EXIT_LOOP,
    MSG_CMD_RESP_PONG, // response pong for ping
    MSG_TYPE_CMD_END,

    MSG_TYPE_CONFIG_START       = 0x1000,
    MSG_CONFIG_REGISTER_ADAPTER = MSG_TYPE_CONFIG_START,
    MSG_CONFIG_UNREGISTER_ADAPTER,
    /*
    MSG_CONFIG_SUBSCRIBE_ADAPTER,
    MSG_CONFIG_ADD_TAGS_GROUP,
    MSG_CONFIG_DEL_TAGS_GROUP,
    MSG_CONFIG_UPDATE_TAGS_GROUP,
    MSG_CONFIG_FIND_TAGS_GROUP,
    */
    MSG_TYPE_CONFIG_END,

    MSG_TYPE_EVENT_START    = 0x2000,
    MSG_EVENT_DRIVER_STATUS = MSG_TYPE_EVENT_START,
    MSG_EVENT_GROUP_CONFIG_CHANGED,
    MSG_EVENT_NODE_PING,
    MSG_TYPE_EVENT_END,

    MSG_TYPE_DATA_START     = 0x3000,
    MSG_DATA_NEURON_DATABUF = MSG_TYPE_DATA_START,
    MSG_TYPE_DATA_END,

    // User customer message types
    MSG_TYPE_VENDOR_START = 0x100000,
    MSG_TYPE_VENDOR_END,

    MSG_TYPE_END = MSG_TYPE_VENDOR_END,

    MSG_DATABUF_KIND_MASK = (0x3 << 29),
    MSG_DATABUF_INPLACE   = (0x0 << 29),
    MSG_DATABUF_EXTERNAL  = (0x1 << 29)
} msg_type_e;

static_assert(MSG_TYPE_END < MSG_DATABUF_KIND_MASK,
              "Too many massage type exceed MSG_DATABUF_KIND_MASK");

/* MSG_CMD_READ_DATA */
typedef struct read_data_cmd {
    neu_taggrp_config_t *grp_config;
    adapter_id_t         sender_id; // adapter_id of sender
    neu_node_id_t        dst_node_id;
} read_data_cmd_t;

/* MSG_CMD_WRITE_DATA */
typedef struct write_data_cmd {
    neu_taggrp_config_t *grp_config;
    adapter_id_t         sender_id; // adapter_id of sender
    neu_node_id_t        dst_node_id;
    core_databuf_t *     databuf;
} write_data_cmd_t;

/* MSG_CMD_START_PERIODIC_READ */
typedef struct start_periodic_read_cmd {
    neu_taggrp_config_t *grp_config;
    adapter_id_t         sender_id; // adapter_id of sender
    neu_node_id_t        dst_node_id;
} start_periodic_read_cmd_t;

/* MSG_CMD_STOP_PERIODIC_READ */
typedef struct stop_periodic_read_cmd {
    neu_taggrp_config_t *grp_config;
    adapter_id_t         sender_id; // adapter_id of sender
    neu_node_id_t        dst_node_id;
} stop_periodic_read_cmd_t;

/* MSG_CMD_SUBSCRIBE_DRIVER */
typedef struct subscribe_driver_cmd {
    neu_taggrp_config_t *grp_config;
    adapter_id_t         sender_id; // adapter_id of sender
    neu_node_id_t        dst_node_id;
} subscribe_node_cmd_t;

/* MSG_CMD_UNSUBSCRIBE_DRIVER */
typedef struct unsubscribe_driver_cmd {
    neu_taggrp_config_t *grp_config;
    adapter_id_t         sender_id; // adapter_id of sender
    neu_node_id_t        dst_node_id;
} unsubscribe_node_cmd_t;

/* MSG_CMD_GROUP_CONFIG_CHANGED */
typedef struct grp_config_changed_cmd {
    neu_taggrp_config_t *grp_config;
    adapter_id_t         sender_id; // adapter_id of sender
    neu_node_id_t        dst_node_id;
} grp_config_changed_cmd_t;

/* MSG_EVENT_GROUP_CONFIG_CHANGED */
typedef struct grp_config_changed_event {
    neu_taggrp_config_t *grp_config;
    adapter_id_t         sender_id; // adapter_id of sender
    neu_node_id_t        dst_node_id;
} grp_config_changed_event_t;

/* MSG_DATA_NEURON_DATABUF */
typedef struct neuron_databuf {
    neu_taggrp_config_t *grp_config;
    core_databuf_t *     databuf;
} neuron_databuf_t;

typedef struct message message_t;

// get size of message
size_t msg_with_struct_get_size(size_t struct_size);

// get size of message
size_t msg_inplace_data_get_size(size_t data_len);

// get size of message
size_t msg_external_data_get_size();

/**
 * Initialize the message, the msg pointer had been allocated by caller, the
 * buffer of struct_ptr had also been allocated by caller. The initialization
 * function will copy contents in struct_ptr to internal buffer pointer.
 */
void msg_with_struct_init(message_t *msg, msg_type_e type, void *struct_ptr,
                          size_t struct_size);
void msg_inplace_data_init(message_t *msg, msg_type_e type, size_t data_len);

/**
 * Initialize the message that has external data buffer. Just set pointer of
 * external buffer to message, and get a strong reference of external buffer.
 */
void msg_external_data_init(message_t *msg, msg_type_e type,
                            core_databuf_t *data_buf);

/**
 *  Uninitialize the message that has external data buffer. Just return strong
 *  reference of external buffer.
 */
void msg_external_data_uninit(message_t *msg);

msg_type_e msg_get_type(message_t *msg);
size_t     msg_get_buf_len(message_t *msg);
void *     msg_get_buf_ptr(message_t *msg);
#endif
