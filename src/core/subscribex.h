/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2022 EMQ Technologies Co., Ltd All rights reserved.
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

#ifndef _NEU_SUBSCRIBE_H_
#define _NEU_SUBSCRIBE_H_

#include <nng/nng.h>

#include "define.h"
#include "utils/utextend.h"

typedef struct neu_subscribe_mgr neu_subscribe_mgr_t;

typedef struct neu_app_subscribe {
    char     app_name[128];
    nng_pipe pipe;
} neu_app_subscribe_t;

neu_subscribe_mgr_t *neu_subscribe_manager_create();
void                 neu_subscribe_manager_destroy(neu_subscribe_mgr_t *mgr);

//  neu_app_subscribe_t array
UT_array *neu_subscribe_manager_find(neu_subscribe_mgr_t *mgr,
                                     const char *driver, const char *group);
UT_array *neu_subscribe_manager_get(neu_subscribe_mgr_t *mgr, const char *app);
int  neu_subscribe_manager_sub(neu_subscribe_mgr_t *mgr, const char *driver,
                               const char *app, const char *group,
                               nng_pipe pipe);
int  neu_subscribe_manager_unsub(neu_subscribe_mgr_t *mgr, const char *driver,
                                 const char *app, const char *group);
void neu_subscribe_manager_remove(neu_subscribe_mgr_t *mgr, const char *driver,
                                  const char *group);

#endif
