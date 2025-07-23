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

#include "utils/asprintf.h"
#include "utils/log.h"
#include "version.h"
#include "json/neu_json_error.h"
#include "json/neu_json_fn.h"

#include "handle.h"
#include "utils/http.h"

/**
 * @brief 处理获取版本信息的HTTP请求
 *
 * 获取当前Neuron的版本信息并以JSON格式返回，包括版本号、Git修订号和构建日期
 *
 * @param aio NNG异步I/O对象，用于处理HTTP请求和响应
 * @return 无返回值，通过aio对象发送响应
 */
void handle_get_version(nng_aio *aio)
{
    char *result = NULL;

    NEU_VALIDATE_JWT(aio);

    int ret = neu_asprintf(
        &result,
        "{\"version\":\"%s\", \"revision\":\"%s\", \"build_date\":\"%s\"}",
        NEURON_VERSION, NEURON_GIT_REV NEURON_GIT_DIFF, NEURON_BUILD_DATE);

    if (ret < 0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_EINTERNAL, {
            neu_http_response(aio, error_code.error, result_error);
        });
        return;
    }

    neu_http_response(aio, 0, result);
    free(result);
}
