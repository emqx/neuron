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

#include "msg.h"
#include "utils/log.h"

#include "msg_internal.h"

void neu_msg_gen(neu_reqresp_head_t *header, void *data)
{
    size_t data_size = neu_reqresp_size(header->type);
    assert(header->len >= sizeof(neu_reqresp_head_t) + data_size);
    memcpy((uint8_t *) &header[1], data, data_size);
}

neu_reqresp_head_t *neu_msg_dup(neu_reqresp_head_t *header)
{
    neu_reqresp_head_t *new_header = calloc(1, header->len);

    *new_header = *header;
    switch (new_header->type) {
    case NEU_REQ_WRITE_TAG: {
        neu_req_write_tag_t *wt     = (neu_req_write_tag_t *) &header[1];
        neu_req_write_tag_t *new_wt = (neu_req_write_tag_t *) &new_header[1];

        *new_wt = *wt;
        break;
    }
    case NEU_REQ_WRITE_TAGS: {
        neu_req_write_tags_t *wts     = (neu_req_write_tags_t *) &header[1];
        neu_req_write_tags_t *new_wts = (neu_req_write_tags_t *) &new_header[1];

        *new_wts = *wts;
        break;
    }
    case NEU_REQ_WRITE_GTAGS: {
        neu_req_write_gtags_t *wgts = (neu_req_write_gtags_t *) &header[1];
        neu_req_write_gtags_t *new_wgts =
            (neu_req_write_gtags_t *) &new_header[1];

        *new_wgts = *wgts;
        break;
    }
    default:
        nlog_warn("unsupport msg type %d", new_header->type);
        assert(false);
    }

    memcpy(&new_header[1], &header[1], header->len - sizeof(*header));

    return new_header;
}
