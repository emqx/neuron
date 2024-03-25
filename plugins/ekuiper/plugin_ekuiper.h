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

#ifndef NEURON_PLUGIN_EKUIPER_H
#define NEURON_PLUGIN_EKUIPER_H

#include <stdint.h>

#include <nng/nng.h>
#include <nng/protocol/pair0/pair.h>
#include <nng/supplemental/util/platform.h>

#include "neuron.h"

#ifdef __cplusplus
extern "C" {
#endif

struct neu_plugin {
    neu_plugin_common_t common;
    nng_socket          sock;
    nng_mtx *           mtx;
    bool                started;
    bool                receiving;
    nng_aio *           recv_aio;
    char *              host;
    uint16_t            port;
    char *              url;
};

#ifdef __cplusplus
}
#endif

#endif
