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

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <nng/nng.h>
#include <nng/supplemental/http/http.h>

#include "http.h"
#include "neu_log.h"

static int response(nng_aio *aio, char *content, enum nng_http_status status)
{
    nng_http_res *res = NULL;

    nng_http_res_alloc(&res);

    nng_http_res_set_header(res, "Content-Type", "application/json");
    nng_http_res_set_header(res, "Access-Control-Allow-Origin", "*");
    nng_http_res_set_header(res, "Access-Control-Allow-Methods",
                            "POST,GET,PUT,DELETE,OPTIONS");
    nng_http_res_set_header(res, "Access-Control-Allow-Headers", "*");

    if (content != NULL && strlen(content) > 0) {
        nng_http_res_copy_data(res, content, strlen(content));
    }

    nng_http_res_set_status(res, status);

    nng_aio_set_output(aio, 0, res);
    nng_aio_finish(aio, 0);

    return 0;
}

const char *http_get_header(nng_aio *aio, char *name)
{
    nng_http_req *req = nng_aio_get_input(aio, 0);

    return nng_http_req_get_header(req, name);
}

int http_get_body(nng_aio *aio, void **data, size_t *data_size)
{
    nng_http_req *req = nng_aio_get_input(aio, 0);

    nng_http_req_get_data(req, data, data_size);
    if (*data_size <= 0) {
        return -1;
    } else {
        char *buf = calloc(*data_size + 1, sizeof(char));
        memcpy(buf, *data, *data_size);
        log_info("body: %s", buf);
        *data = buf;
        return 0;
    }
}

static char *get_param(const char *url, const char *name)
{
    char        param[32]   = { 0 };
    static char s_value[64] = { 0 };
    int         len         = snprintf(param, sizeof(param), "%s=", name);

    const char *find = strstr(url, param);
    if (find == NULL) {
        return NULL;
    }

    for (uint8_t i = 0; i < 64 && i < strlen(find); i++) {
        if (*(find + len + i) == '&' || *(find + len + i) == '\0') {
            s_value[i] = '\0';
            break;
        }
        s_value[i] = *(find + len + i);
    }

    return s_value;
}

char *http_get_param(nng_aio *aio, const char *name)
{
    nng_url *     nn_url         = NULL;
    char          parse_url[256] = { 0 };
    nng_http_req *nng_req        = nng_aio_get_input(aio, 0);
    int           ret            = -1;
    char *        result         = NULL;

    snprintf(parse_url, sizeof(parse_url), "http://127.0.0.1:7000/%s",
             nng_http_req_get_uri(nng_req));

    ret = nng_url_parse(&nn_url, parse_url);
    if (ret != 0 || nn_url->u_query == NULL) {
        return NULL;
    }

    result = get_param(nn_url->u_query, name);

    nng_url_free(nn_url);

    return result;
}

int http_get_param_int(nng_aio *aio, const char *name, int32_t *param)
{
    char *   tmp    = http_get_param(aio, name);
    intmax_t result = 0;

    if (tmp == NULL) {
        return -1;
    }

    result = strtoimax(tmp, NULL, 10);
    if (result == INTMAX_MAX && errno == ERANGE) {
        return -1;
    }

    *param = result;

    return 0;
}

int http_response(nng_aio *aio, neu_err_code_e code, char *content)
{
    enum nng_http_status status = NNG_HTTP_STATUS_OK;

    switch (code) {
    case NEU_ERR_SUCCESS:
    case NEU_ERR_NODE_SETTING_NOT_FOUND:
        status = NNG_HTTP_STATUS_OK;
        break;
    case NEU_ERR_EINTERNAL:
        status = NNG_HTTP_STATUS_INTERNAL_SERVER_ERROR;
        break;
    case NEU_ERR_EXPIRED_TOKEN:
    case NEU_ERR_VALIDATE_TOKEN:
    case NEU_ERR_INVALID_TOKEN:
        status = NNG_HTTP_STATUS_FORBIDDEN;
        break;
    case NEU_ERR_NEED_TOKEN:
    case NEU_ERR_DECODE_TOKEN:
    case NEU_ERR_INVALID_USER_OR_PASSWORD:
        status = NNG_HTTP_STATUS_UNAUTHORIZED;
        break;
    case NEU_ERR_BODY_IS_WRONG:
    case NEU_ERR_PARAM_IS_WRONG:
    case NEU_ERR_NODE_TYPE_INVALID:
    case NEU_ERR_NODE_SETTING_INVALID:
        status = NNG_HTTP_STATUS_BAD_REQUEST;
        break;
    case NEU_ERR_PLUGIN_NAME_NOT_FOUND:
    case NEU_ERR_NODE_NOT_EXIST:
    case NEU_ERR_GRP_CONFIG_NOT_EXIST:
    case NEU_ERR_TAG_NOT_EXIST:
    case NEU_ERR_GRP_NOT_SUBSCRIBE:
        status = NNG_HTTP_STATUS_NOT_FOUND;
        break;
    case NEU_ERR_NODE_EXIST:
    case NEU_ERR_NODE_NOT_READY:
    case NEU_ERR_NODE_IS_RUNNING:
    case NEU_ERR_NODE_NOT_RUNNING:
    case NEU_ERR_NODE_IS_STOPED:
    case NEU_ERR_TAG_NAME_EXIST:
    case NEU_ERR_GRP_CONFIG_EXIST:
        status = NNG_HTTP_STATUS_CONFLICT;
        break;
    case NEU_ERR_TAG_ATTRIBUTE_NOT_SUPPORT:
    case NEU_ERR_TAG_TYPE_NOT_SUPPORT:
        status = NNG_HTTP_STATUS_PARTIAL_CONTENT;
        break;
    case NEU_ERR_GRP_CONFIG_IN_USE:
        status = NNG_HTTP_STATUS_PRECONDITION_FAILED;
        break;
    default:
        log_error("unhandle error code: %d", code);
        assert(code == 0);
        break;
    }

    return response(aio, content, status);
}

int http_ok(nng_aio *aio, char *content)
{
    return response(aio, content, NNG_HTTP_STATUS_OK);
}

int http_created(nng_aio *aio, char *content)
{
    return response(aio, content, NNG_HTTP_STATUS_CREATED);
}

int http_partial(nng_aio *aio, char *content)
{
    return response(aio, content, NNG_HTTP_STATUS_PARTIAL_CONTENT);
}

int http_bad_request(nng_aio *aio, char *content)
{
    return response(aio, content, NNG_HTTP_STATUS_BAD_REQUEST);
}

int http_unauthorized(nng_aio *aio, char *content)
{
    return response(aio, content, NNG_HTTP_STATUS_UNAUTHORIZED);
}

int http_not_found(nng_aio *aio, char *content)
{
    return response(aio, content, NNG_HTTP_STATUS_NOT_FOUND);
}

int http_conflict(nng_aio *aio, char *content)
{
    return response(aio, content, NNG_HTTP_STATUS_CONFLICT);
}

int http_internal_error(nng_aio *aio, char *content)
{
    return response(aio, content, NNG_HTTP_STATUS_INTERNAL_SERVER_ERROR);
}
