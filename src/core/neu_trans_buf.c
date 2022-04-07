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

#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>

#include "csptr/smart_ptr.h"
#include "log.h"

#include "core/databuf.h"
#include "core/message.h"
#include "core/neu_trans_buf.h"

neu_trans_buf_t *neu_trans_buf_new(neu_trans_kind_e trans_kind,
                                   neu_data_val_t * data_val)
{
    int              rv;
    neu_trans_buf_t *trans_buf;

    trans_buf = (neu_trans_buf_t *) malloc(sizeof(neu_trans_buf_t));
    if (trans_buf == NULL) {
        log_error("Failed to allocate trans data");
        neu_dvalue_free(data_val);
        return NULL;
    }

    rv = neu_trans_buf_init(trans_buf, trans_kind, data_val);
    if (rv != 0) {
        return NULL;
    }

    return trans_buf;
}

void neu_trans_buf_free(neu_trans_buf_t *trans_buf)
{
    if (trans_buf == NULL) {
        log_error("Free neuron trans_buf with NULL pointer");
        return;
    }

    neu_trans_buf_uninit(trans_buf);
    free(trans_buf);
}

static void dvalue_dtor(void *p_data_val, void *meta)
{
    (void) meta;

    neu_dvalue_free(*(neu_data_val_t **) p_data_val);
    return;
}

int neu_trans_buf_init(neu_trans_buf_t *trans_buf, neu_trans_kind_e trans_kind,
                       neu_data_val_t *data_val)
{
    int             rv = 0;
    size_t          buf_len;
    uint8_t *       buf;
    core_databuf_t *databuf;
    neu_data_val_t *owned_data_val;

    if (trans_buf == NULL || data_val == NULL) {
        log_error("Init neuron trans_buf with NULL pointer");
        neu_dvalue_free(data_val);
        return -1;
    }

    trans_buf->trans_kind = trans_kind;
    switch (trans_kind) {
    case NEURON_TRANS_DATABUF:
        buf_len = neu_dvalue_serialize(data_val, &buf);
        databuf = core_databuf_new_with_buf(buf, buf_len);
        neu_dvalue_free(data_val);
        if (databuf == NULL) {
            log_error(
                "Failed to serialize data value(%p) or new buf with len(%d)",
                data_val, buf_len);
            rv = -1;
            break;
        }

        trans_buf->databuf = databuf;
        break;
    case NEURON_TRANS_DATAVAL:
        owned_data_val = neu_dvalue_to_owned(data_val);
        if (owned_data_val == NULL) {
            neu_dvalue_free(data_val);
            log_error("Failed to owner the data value(%p)", data_val);
            rv = -1;
            break;
        }

        trans_buf->shared_dataval =
            shared_ptr(void *, owned_data_val, dvalue_dtor);
        break;
    default:
        neu_dvalue_free(data_val);
        log_error("Not support trans data kind: %d", trans_kind);
        rv = -1;
        break;
    }

    return rv;
}

void neu_trans_buf_uninit(neu_trans_buf_t *trans_buf)
{
    neu_trans_kind_e trans_kind;

    if (trans_buf == NULL) {
        log_error("Uninit neuron trans_buf with NULL pointer");
        return;
    }

    trans_kind = trans_buf->trans_kind;
    switch (trans_kind) {
    case NEURON_TRANS_DATABUF:
        core_databuf_put(trans_buf->databuf);
        break;
    case NEURON_TRANS_DATAVAL:
        sfree(trans_buf->shared_dataval);
        break;
    default:
        log_error("Not support trans data kind: %d", trans_kind);
        break;
    }

    return;
}

neu_data_val_t *neu_trans_buf_get_data_val(neu_trans_buf_t *trans_buf,
                                           bool *           is_need_free)
{
    neu_trans_kind_e trans_kind;
    neu_data_val_t * data_val = NULL;

    if (trans_buf == NULL || is_need_free == NULL) {
        log_error("Get data value from trans_buf with NULL pointer");
        return NULL;
    }

    *is_need_free = false;
    trans_kind    = trans_buf->trans_kind;
    switch (trans_kind) {
    case NEURON_TRANS_DATABUF: {
        void * buf     = core_databuf_get_ptr(trans_buf->databuf);
        size_t buf_len = core_databuf_get_len(trans_buf->databuf);
        neu_dvalue_deserialize(buf, buf_len, &data_val);
        *is_need_free = true;
        break;
    }
    case NEURON_TRANS_DATAVAL:
        data_val      = *(neu_data_val_t **) (trans_buf->shared_dataval);
        *is_need_free = false;
        break;
    default:
        log_error("Not support trans data kind: %d", trans_kind);
        break;
    }

    return data_val;
}

void neu_trans_buf_copy(neu_trans_buf_t *dst, neu_trans_buf_t *src)
{
    neu_trans_kind_e trans_kind;

    if (dst == NULL || src == NULL) {
        log_error("Copy trans data with NULL pointer");
        return;
    }

    dst->trans_kind = src->trans_kind;
    trans_kind      = src->trans_kind;
    switch (trans_kind) {
    case NEURON_TRANS_DATABUF:
        dst->databuf = core_databuf_get(src->databuf);
        break;
    case NEURON_TRANS_DATAVAL:
        dst->shared_dataval = sref(src->shared_dataval);
        break;
    default:
        log_error("Not support trans data kind: %d", trans_kind);
        break;
    }

    return;
}
