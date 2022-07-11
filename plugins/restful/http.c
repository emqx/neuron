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

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <nng/nng.h>
#include <nng/supplemental/http/http.h>

#include "errcodes.h"
#include "http.h"
#include "utils/log.h"

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

    nng_http_req *nng_req = nng_aio_get_input(aio, 0);
    nlog_notice("%s %s [%d]", nng_http_req_get_method(nng_req),
                nng_http_req_get_uri(nng_req), status);

    nng_aio_set_output(aio, 0, res);
    nng_aio_finish(aio, 0);

    return 0;
}

ssize_t url_decode(const char *s, size_t len, char *buf, size_t size)
{
    size_t       i = 0, j = 0;
    int          n = 0;
    unsigned int c;
    while (i < len && j < size) {
        c = s[i++];
        if ('+' == c) {
            c = ' ';
        } else if ('%' == c) {
            if (i + 2 > len || !sscanf(s + i, "%2x%n", &c, &n) || 2 != n) {
                return -1;
            }
            i += 2, n = 0;
        }
        buf[j++] = c;
    }
    buf[(j < size) ? j : --j] = '\0';
    return j;
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
    if (*data_size == 0) {
        return -1;
    } else {
        char *buf = calloc(*data_size + 1, sizeof(char));
        memcpy(buf, *data, *data_size);
        *data = buf;
        return 0;
    }
}

// Find query parameter value of the given name.
//
// On failure, returns NULL.
// On success, returns an alias pointer to the value following name in the url,
// if `len_p` is not NULL, then stores the length of the value in `*len_p`.
//
// Example
// -------
// 1. get_param("/?key=val", "key", &len) will return a pointer to "val"
//    and store 3 in len.
// 2. get_param("/?key&foo", "key", &len) will return a pointer to "&foo"
//    and store 0 in len.
// 3. get_param("/?foo=bar", "key", &len) will return NULL and will not
//    touch len.
static char *get_param(const char *url, const char *name, size_t *len_p)
{
    const char *query = strchr(url, '?');

    if (NULL == query) {
        return NULL;
    }

    int   len = strlen(name);
    char *p   = strstr(query, name);
    while (NULL != p) {
        if (0 == p[len] || '&' == p[len] || '=' == p[len] || '#' == p[len]) {
            break;
        }
        p = strstr(p + len, name);
    }

    if (NULL == p) {
        return NULL;
    }

    const char *frag = strchr(url, '#');
    if (frag && p > frag) {
        return NULL;
    }

    p += len;

    if ('=' != *p) {
        if (NULL != len_p) {
            *len_p = 0;
        }
        return p;
    }

    ++p;

    if (NULL == len_p) {
        return p;
    }

    int i = 0;
    while (p[i] && '&' != p[i]) {
        ++i;
    }
    *len_p = i;

    return p;
}

const char *http_get_param(nng_aio *aio, const char *name, size_t *len)
{
    nng_http_req *nng_req = nng_aio_get_input(aio, 0);

    const char *uri = nng_http_req_get_uri(nng_req);
    const char *val = get_param(uri, name, len);

    return val;
}

ssize_t http_get_param_str(nng_aio *aio, const char *name, char *buf,
                           size_t size)
{
    size_t      len = 0;
    const char *s   = http_get_param(aio, name, &len);
    if (NULL == s) {
        return -2;
    }
    return url_decode(s, len, buf, size);
}

int http_get_param_uintmax(nng_aio *aio, const char *name, uintmax_t *param)
{
    size_t      len    = 0;
    const char *tmp    = http_get_param(aio, name, &len);
    char *      end    = NULL;
    uintmax_t   result = 0;

    if (tmp == NULL) {
        return -1;
    }

    if (0 == len) {
        return -1;
    }

    result = strtoumax(tmp, &end, 10);
    if ((result == UINTMAX_MAX && errno == ERANGE) || result > UINTMAX_MAX ||
        tmp + len != end) {
        return -1;
    }

    *param = result;

    return 0;
}

int http_get_param_uint64(nng_aio *aio, const char *name, uint64_t *param)
{
    uintmax_t val;

    int rv = http_get_param_uintmax(aio, name, &val);
    if (0 != rv) {
        return rv;
    }

    if (val > UINT64_MAX) {
        return -1;
    }

    *param = (uint64_t) val;
    return 0;
}

int http_get_param_uint32(nng_aio *aio, const char *name, uint32_t *param)
{
    uintmax_t val;

    int rv = http_get_param_uintmax(aio, name, &val);
    if (0 != rv) {
        return rv;
    }

    if (val > UINT32_MAX) {
        return -1;
    }

    *param = (uint32_t) val;
    return 0;
}

int http_get_param_node_type(nng_aio *aio, const char *name,
                             neu_node_type_e *param)
{
    uintmax_t val;

    int rv = http_get_param_uintmax(aio, name, &val);
    if (0 != rv) {
        return rv;
    }

    if (val != NEU_NA_TYPE_APP && val != NEU_NA_TYPE_DRIVER) {
        return -1;
    }

    *param = (neu_node_type_e) val;
    return 0;
}

int http_response(nng_aio *aio, neu_err_code_e code, char *content)
{
    enum nng_http_status status = NNG_HTTP_STATUS_OK;

    switch (code) {
    case NEU_ERR_SUCCESS:
    case NEU_ERR_NODE_SETTING_NOT_FOUND:
    case NEU_ERR_PLUGIN_READ_FAILURE:
    case NEU_ERR_PLUGIN_WRITE_FAILURE:
    case NEU_ERR_PLUGIN_DISCONNECTED:
    case NEU_ERR_PLUGIN_TAG_NOT_ALLOW_READ:
    case NEU_ERR_PLUGIN_TAG_NOT_ALLOW_WRITE:
    case NEU_ERR_LICENSE_EXPIRED:
    case NEU_ERR_LICENSE_DISABLED:
    case NEU_ERR_LICENSE_MAX_NODES:
    case NEU_ERR_LICENSE_MAX_TAGS:
    case NEU_ERR_GROUP_ALREADY_SUBSCRIBED:
    case NEU_ERR_PLUGIN_TAG_TYPE_MISMATCH:
    case NEU_ERR_PLUGIN_PROTOCOL_DECODE_FAILURE:
        status = NNG_HTTP_STATUS_OK;
        break;
    case NEU_ERR_EINTERNAL:
    case NEU_ERR_IS_BUSY:
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
    case NEU_ERR_NODE_SETTING_INVALID:
    case NEU_ERR_LIBRARY_INFO_INVALID:
    case NEU_ERR_LICENSE_INVALID:
    case NEU_ERR_GROUP_PARAMETER_INVALID:
    case NEU_ERR_LIBRARY_SYSTEM_NOT_ALLOW_DEL:
    case NEU_ERR_LIBRARY_FAILED_TO_OPEN:
    case NEU_ERR_LIBRARY_MODULE_INVALID:
    case NEU_ERR_NODE_NAME_TOO_LONG:
    case NEU_ERR_GROUP_NAME_TOO_LONG:
        status = NNG_HTTP_STATUS_BAD_REQUEST;
        break;
    case NEU_ERR_FILE_NOT_EXIST:
    case NEU_ERR_LIBRARY_NOT_FOUND:
    case NEU_ERR_NODE_NOT_EXIST:
    case NEU_ERR_TAG_NOT_EXIST:
    case NEU_ERR_LICENSE_NOT_FOUND:
    case NEU_ERR_GROUP_NOT_EXIST:
    case NEU_ERR_GROUP_NOT_SUBSCRIBE:
        status = NNG_HTTP_STATUS_NOT_FOUND;
        break;
    case NEU_ERR_NODE_EXIST:
    case NEU_ERR_NODE_NOT_READY:
    case NEU_ERR_NODE_IS_RUNNING:
    case NEU_ERR_NODE_NOT_RUNNING:
    case NEU_ERR_NODE_IS_STOPED:
    case NEU_ERR_TAG_NAME_CONFLICT:
    case NEU_ERR_LIBRARY_NAME_CONFLICT:
    case NEU_ERR_GROUP_EXIST:
    case NEU_ERR_GROUP_NOT_ALLOW:
        status = NNG_HTTP_STATUS_CONFLICT;
        break;
    case NEU_ERR_TAG_ATTRIBUTE_NOT_SUPPORT:
    case NEU_ERR_TAG_TYPE_NOT_SUPPORT:
    case NEU_ERR_TAG_ADDRESS_FORMAT_INVALID:
        status = NNG_HTTP_STATUS_PARTIAL_CONTENT;
        break;
    default:
        if (code >= NEU_ERR_PLUGIN_ERROR_START &&
            code <= NEU_ERR_PLUGIN_ERROR_END) {
            status = NNG_HTTP_STATUS_OK;
        } else {
            nlog_error("unhandle error code: %d", code);
            assert(code == 0);
        }
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
