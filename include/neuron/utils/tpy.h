/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2025 EMQ Technologies Co., Ltd All rights reserved.
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

#ifndef _NEU_UTILS_TPY_H_
#define _NEU_UTILS_TPY_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "define.h"
#include "msg.h"

typedef struct {
    int64_t igroup;
    int64_t ioffset;
    char    name[NEU_TAG_NAME_LEN];
    char    type[NEU_TAG_NAME_LEN];
} tpy_var_t;

typedef struct {
    char name[NEU_CID_IED_NAME_LEN];

    int        n_vars;
    tpy_var_t *vars;
} tpy_static_t;

typedef struct {
    int           n_statics;
    tpy_static_t *statics;
} tpy_t;

int  neu_tpy_parse(const char *path, tpy_t *tpy);
void neu_tpy_free(tpy_t *tpy);

void neu_tpy_to_msg(char *driver, tpy_t *tpy, neu_req_add_gtag_t *cmd);

#ifdef __cplusplus
}
#endif

#endif