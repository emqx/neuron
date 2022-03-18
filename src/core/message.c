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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "core/databuf.h"
#include "core/message.h"

typedef struct msg_inplace_buf {
    size_t buf_len;
    // The buf_ptr pointer to msg_buf
    void *buf_ptr;
} msg_inplace_buf_t;

struct message {
    msg_type_e type;
    uint32_t   msg_id;
    union {
        msg_inplace_buf_t inp_buf;
        // The external buffer pointer to a core_databuf_t.
        core_databuf_t *ext_buf;
    };

    // For align with 8 bytes
    uint64_t msg_buf[0];
    // Don’t add any member after here
};

// get size of message
size_t msg_with_struct_get_size(size_t struct_size)
{
    return sizeof(message_t) + struct_size;
}

// get size of message
size_t msg_inplace_data_get_size(size_t data_len)
{
    return sizeof(message_t) + data_len;
}

// get size of message
size_t msg_external_data_get_size()
{
    return sizeof(message_t);
}

void msg_with_struct_init(message_t *msg, msg_type_e type, void *struct_ptr,
                          size_t struct_size)
{
    if (msg == NULL || (struct_ptr == NULL && 0 != struct_size)) {
        log_error("msg_with_struct_init called with NULL pointr");
        return;
    }

    msg->type            = type;
    msg->inp_buf.buf_len = struct_size;
    msg->inp_buf.buf_ptr = msg->msg_buf;
    memcpy(msg->msg_buf, struct_ptr, struct_size);
    return;
}

void msg_inplace_data_init(message_t *msg, msg_type_e type, size_t data_len)
{
    if (msg == NULL) {
        log_error("msg_inplace_data_init called with NULL pointr");
        return;
    }

    msg->type            = type;
    msg->inp_buf.buf_len = data_len;
    msg->inp_buf.buf_ptr = msg->msg_buf;
}

void msg_external_data_init(message_t *msg, msg_type_e type,
                            core_databuf_t *data_buf)
{
    if (msg == NULL) {
        log_error("msg_external_data_init called with NULL pointr");
        return;
    }

    msg->type    = type;
    msg->ext_buf = core_databuf_get(data_buf);
    return;
}

void msg_external_data_uninit(message_t *msg)
{
    if (!(msg != NULL && msg->ext_buf != NULL)) {
        log_error("msg_external_data_uninit called with NULL pointr");
        return;
    }

    core_databuf_put(msg->ext_buf);
    msg->ext_buf = NULL;
}

msg_type_e msg_get_type(message_t *msg)
{
    if (msg == NULL) {
        return MSG_CMD_NOP;
    }

    return msg->type;
}

size_t msg_get_buf_len(message_t *msg)
{
    if (msg == NULL) {
        return 0;
    }

    if ((msg->type & MSG_DATABUF_KIND_MASK) == MSG_DATABUF_EXTERNAL) {
        return core_databuf_get_len(msg->ext_buf);
    } else {
        return msg->inp_buf.buf_len;
    }
}

void *msg_get_buf_ptr(message_t *msg)
{
    if (msg == NULL) {
        return NULL;
    }

    if ((msg->type & MSG_DATABUF_KIND_MASK) == MSG_DATABUF_EXTERNAL) {
        return core_databuf_get_ptr(msg->ext_buf);
    } else {
        return msg->inp_buf.buf_ptr;
    }
}

nng_msg *nng_msg_inplace_from_buf(msg_type_e msg_type, void *buf, size_t size)
{
    nng_msg *msg      = NULL;
    size_t   msg_size = msg_inplace_data_get_size(size);
    int      rv       = nng_msg_alloc(&msg, msg_size);
    if (rv == 0) {
        message_t *msg_ptr = nng_msg_body(msg);
        msg_with_struct_init(msg_ptr, msg_type, buf, size);
    }
    return msg;
}
