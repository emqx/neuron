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

#ifndef NEURON_TRANS_BUF_H
#define NEURON_TRANS_BUF_H

#include "data_expr.h"
#include "databuf.h"

typedef enum neu_trans_kind {
    NEURON_TRANS_DATABUF,
    NEURON_TRANS_DATAVAL,
} neu_trans_kind_e;

/*
 * A databuf or shared data value for transfer data to other adapter
 */
typedef struct neu_trans_buf {
    neu_trans_kind_e trans_kind;
    union {
        core_databuf_t *databuf;
        void *          shared_dataval;
    };
} neu_trans_buf_t;

/**
 * New a transfer buf with data value, the ownership of data value will be
 * move to this function, and free data value in this function.
 */
neu_trans_buf_t *neu_trans_buf_new(neu_trans_kind_e trans_kind,
                                   neu_data_val_t * data_val);
void             neu_trans_buf_free(neu_trans_buf_t *trans_buf);

/**
 * Initialize a transfer buf with data value, the ownership of data value will
 * be move to this function, and free data value in this function.
 */
int neu_trans_buf_init(neu_trans_buf_t *trans_buf, neu_trans_kind_e trans_kind,
                       neu_data_val_t *data_val);

void            neu_trans_buf_uninit(neu_trans_buf_t *trans_buf);
neu_data_val_t *neu_trans_buf_get_data_val(neu_trans_buf_t *trans_buf,
                                           bool *           is_need_free);
void            neu_trans_buf_copy(neu_trans_buf_t *dst, neu_trans_buf_t *src);

#endif
