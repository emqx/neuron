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

#ifndef _NEU_ADAPTER_H_
#define _NEU_ADAPTER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include <sys/un.h>

#include "define.h"
#include "metrics.h"
#include "msg.h"

typedef struct neu_adapter_driver neu_adapter_driver_t;
typedef struct neu_adapter_app    neu_adapter_app_t;

typedef int (*neu_adapter_update_metric_cb_t)(neu_adapter_t *adapter,
                                              const char *   metric_name,
                                              uint64_t n, const char *group);
typedef int (*neu_adapter_register_metric_cb_t)(neu_adapter_t *   adapter,
                                                const char *      name,
                                                const char *      help,
                                                neu_metric_type_e type,
                                                uint64_t          init);

typedef struct adapter_callbacks {
    int (*command)(neu_adapter_t *adapter, neu_reqresp_head_t head, void *data);
    int (*response)(neu_adapter_t *adapter, neu_reqresp_head_t *head,
                    void *data);
    int (*responseto)(neu_adapter_t *adapter, neu_reqresp_head_t *head,
                      void *data, struct sockaddr_un dst);
    neu_adapter_register_metric_cb_t register_metric;
    neu_adapter_update_metric_cb_t   update_metric;

    union {
        struct {
            void (*update)(neu_adapter_t *adapter, const char *group,
                           const char *tag, neu_dvalue_t value);
            void (*update_with_meta)(neu_adapter_t *adapter, const char *group,
                                     const char *tag, neu_dvalue_t value,
                                     neu_tag_meta_t *metas, int n_meta);
            void (*write_response)(neu_adapter_t *adapter, void *req,
                                   int error);
            void (*update_im)(neu_adapter_t *adapter, const char *group,
                              const char *tag, neu_dvalue_t value,
                              neu_tag_meta_t *metas, int n_meta);
            void (*scan_tags_response)(neu_adapter_t *adapter, void *r,
                                       neu_resp_scan_tags_t *resp_scan);
            void (*test_read_tag_response)(neu_adapter_t *adapter, void *r,
                                           neu_json_type_e t, neu_type_e type,
                                           neu_json_value_u value,
                                           int64_t          error);
        } driver;
    };
} adapter_callbacks_t;

#ifdef __cplusplus
}
#endif

#endif