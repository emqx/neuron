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

#ifndef _NEURON_H_
#define _NEURON_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "errcodes.h"

#include "utils/async_queue.h"
#include "utils/protocol_buf.h"

#include "utils/base64.h"
#include "utils/log.h"
#include "utils/zlog.h"

#include "utils/neu_jwt.h"
#include "utils/time.h"
#include "utils/utarray.h"
#include "utils/utextend.h"
#include "utils/uthash.h"
#include "utils/utlist.h"

#include "connection/neu_connection.h"
#include "connection/neu_connection_eth.h"
#include "event/event.h"

#include "json/json.h"
#include "json/neu_json_error.h"
#include "json/neu_json_fn.h"
#include "json/neu_json_mqtt.h"
#include "json/neu_json_param.h"
#include "json/neu_json_rw.h"

#include "define.h"

#include "adapter.h"
#include "plugin.h"
#include "tag.h"
#include "tag_sort.h"

#ifdef __cplusplus
}
#endif

#endif
