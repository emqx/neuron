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

#ifndef _NEU_ADAPTER_DRIVER_INTERNAL_H_
#define _NEU_ADAPTER_DRIVER_INTERNAL_H_

#include "adapter.h"

neu_adapter_driver_t *neu_adapter_driver_create();

void neu_adapter_driver_destroy(neu_adapter_driver_t *driver);
int  neu_adapter_driver_init(neu_adapter_driver_t *driver);
int  neu_adapter_driver_uninit(neu_adapter_driver_t *driver);

void neu_adapter_driver_start_group_timer(neu_adapter_driver_t *driver);
void neu_adapter_driver_stop_group_timer(neu_adapter_driver_t *driver);

void neu_adapter_driver_read_group(neu_adapter_driver_t *driver,
                                   neu_reqresp_head_t *  req);
void neu_adapter_driver_read_group_paginate(neu_adapter_driver_t *driver,
                                            neu_reqresp_head_t *  req);
void neu_adapter_driver_test_read_tag(neu_adapter_driver_t *driver,
                                      neu_reqresp_head_t *  req);

void neu_adapter_driver_write_tag(neu_adapter_driver_t *driver,
                                  neu_reqresp_head_t *  req);
void neu_adapter_driver_write_tags(neu_adapter_driver_t *driver,
                                   neu_reqresp_head_t *  req);
void neu_adapter_driver_write_gtags(neu_adapter_driver_t *driver,
                                    neu_reqresp_head_t *  req);

int neu_adapter_driver_add_group(neu_adapter_driver_t *driver, const char *name,
                                 uint32_t interval);
int neu_adapter_driver_update_group(neu_adapter_driver_t *driver,
                                    const char *name, const char *new_name,
                                    uint32_t interval);
int neu_adapter_driver_del_group(neu_adapter_driver_t *driver,
                                 const char *          name);
int neu_adapter_driver_group_exist(neu_adapter_driver_t *driver,
                                   const char *          name);
UT_array *neu_adapter_driver_get_group(neu_adapter_driver_t *driver);
uint16_t  neu_adapter_driver_group_count(neu_adapter_driver_t *driver);
uint16_t  neu_adapter_driver_new_group_count(neu_adapter_driver_t *driver,
                                             neu_req_add_gtag_t *  cmd);

int neu_adapter_driver_try_del_tag(neu_adapter_driver_t *driver, int n_tag);
int neu_adapter_driver_try_add_tag(neu_adapter_driver_t *driver,
                                   const char *group, neu_datatag_t *tags,
                                   int n_tag);
int neu_adapter_driver_load_tag(neu_adapter_driver_t *driver, const char *group,
                                neu_datatag_t *tags, int n_tag);

int neu_adapter_driver_validate_tag(neu_adapter_driver_t *driver,
                                    const char *group, neu_datatag_t *tag);

int neu_adapter_driver_add_tag(neu_adapter_driver_t *driver, const char *group,
                               neu_datatag_t *tag, uint16_t interval);
int neu_adapter_driver_del_tag(neu_adapter_driver_t *driver, const char *group,
                               const char *tag);
int neu_adapter_driver_update_tag(neu_adapter_driver_t *driver,
                                  const char *group, neu_datatag_t *tag);
int neu_adapter_driver_get_tag(neu_adapter_driver_t *driver, const char *group,
                               UT_array **tags);
UT_array *neu_adapter_driver_get_ptag(neu_adapter_driver_t *driver,
                                      const char *group, const char *tag);
int       neu_adapter_driver_query_tag(neu_adapter_driver_t *driver,
                                       const char *group, const char *name,
                                       UT_array **tags);
void      neu_adapter_driver_get_value_tag(neu_adapter_driver_t *driver,
                                           const char *group, UT_array **tags);
UT_array *neu_adapter_driver_get_read_tag(neu_adapter_driver_t *driver,
                                          const char *          group);

void neu_adapter_driver_subscribe(neu_adapter_driver_t *driver,
                                  neu_req_subscribe_t * req);
void neu_adapter_driver_unsubscribe(neu_adapter_driver_t * driver,
                                    neu_req_unsubscribe_t *req);

void neu_adapter_driver_scan_tags(neu_adapter_driver_t *driver,
                                  neu_reqresp_head_t *  req);
#endif
