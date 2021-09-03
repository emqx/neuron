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

#ifndef _NEU_PLUGIN_REST_HANDLE_H_
#define _NEU_PLUGIN_REST_HANDLE_H_

#include "neu_plugin.h"

enum neu_rest_method {
    NEU_REST_METHOD_GET = 0x0,
    NEU_REST_METHOD_POST,
    NEU_REST_METHOD_PUT,
    NEU_REST_METHOD_DELETE,
    NEU_REST_METHOD_OPTIONS
};
enum neu_rest_handler_type {
    NEU_REST_HANDLER_FUNCTION = 0x0,
    NEU_REST_HANDLER_DIRECTORY,
};

struct neu_rest_handler {
    enum neu_rest_method       method;
    char *                     url;
    enum neu_rest_handler_type type;
    union neu_rest_handler_value {
        void *handler;
        char *path;
    } value;
};

void neu_rest_init_all_handler(const struct neu_rest_handler **handlers,
                               uint32_t *size, neu_plugin_t *plugin);

#endif
