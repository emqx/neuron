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

#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <jwt.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "persist/persist.h"
#include "user.h"
#include "utils/asprintf.h"
#include "utils/log.h"

#include "argparse.h"
#include "parser/neu_json_otel.h"
#include "json/neu_json_fn.h"

#include "handle.h"
#include "utils/http.h"
#include "utils/neu_jwt.h"

#include "otel/otel_manager.h"
#include "otel_handle.h"

void handle_otel(nng_aio *aio)
{
    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_otel_conf_req_t, neu_json_decode_otel_conf_req, {
            if (strcmp(req->action, "start") == 0) {
                otel_flag = true;
                strcpy(otel_collector_url, req->collector_url);
                otel_control_flag     = req->control_flag;
                otel_data_flag        = req->data_flag;
                otel_data_sample_rate = req->data_sample_rate;
                neu_http_ok(aio, "{\"error\": 0 }");
                neu_otel_start();
            } else if (strcmp(req->action, "stop") == 0) {
                otel_flag = false;
                neu_http_ok(aio, "{\"error\": 0 }");
                neu_otel_stop();
            } else {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_PARAM_IS_WRONG, {
                    neu_http_response(aio, NEU_ERR_PARAM_IS_WRONG,
                                      result_error);
                });
            }
        })
}