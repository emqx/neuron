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

#define _POSIX_C_SOURCE 200809L
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "errcodes.h"
#include "parser/neu_json_license.h"
#include "plugin.h"
#include "utils/log.h"
#include "json/neu_json_error.h"
#include "json/neu_json_fn.h"

#include "handle.h"
#include "http.h"
#include "license.h"
#include "license_handle.h"

#define LICENSE_PATH LICENSE_DIR "/" LICENSE_FNAME

static int set_license(const char *lic_str);
int        backup_license_file(const char *lic_fname);
int        get_plugin_names(const license_t *lic, UT_array *plugin_names);

void handle_get_license(nng_aio *aio)
{
    int                         rv              = 0;
    license_t                   lic             = {};
    neu_json_get_license_resp_t resp            = {};
    time_t                      since           = 0;
    time_t                      until           = 0;
    char                        valid_since[20] = {};
    char                        valid_until[20] = {};
    struct tm                   tm              = {};
    UT_array *                  plugin_names    = NULL;
    char *                      result          = NULL;

    VALIDATE_JWT(aio);

    utarray_new(plugin_names, &ut_ptr_icd);
    license_init(&lic);
    rv = license_read(&lic, LICENSE_PATH);
    if (0 != rv) {
        goto final;
    }

    since = license_not_before(&lic);
    gmtime_r(&since, &tm);
    if (0 ==
        strftime(valid_since, sizeof(valid_since), "%Y-%m-%d %H:%M:%S", &tm)) {
        rv = NEU_ERR_EINTERNAL;
        goto final;
    }

    until = license_not_after(&lic);
    gmtime_r(&until, &tm);
    if (0 ==
        strftime(valid_until, sizeof(valid_until), "%Y-%m-%d %H:%M:%S", &tm)) {
        rv = NEU_ERR_EINTERNAL;
        goto final;
    }

    resp.license_type  = (char *) license_type_str(license_get_type(&lic));
    resp.valid         = !license_is_expired(&lic);
    resp.valid_since   = valid_since;
    resp.valid_until   = valid_until;
    resp.max_nodes     = license_get_max_nodes(&lic);
    resp.max_node_tags = license_get_max_node_tags(&lic);

    rv = get_plugin_names(&lic, plugin_names);
    if (0 != rv) {
        goto final;
    }
    resp.n_enabled_plugin = utarray_len(plugin_names);
    utarray_foreach(plugin_names, char **, p_name)
    {
        int index                   = utarray_eltidx(plugin_names, p_name);
        resp.enabled_plugins[index] = *p_name;
    }

final:
    if (0 == rv) {
        neu_json_encode_by_fn(&resp, neu_json_encode_get_license_resp, &result);
    } else {
        neu_json_error_resp_t error_code = { .error = rv };
        neu_json_encode_by_fn(&error_code, neu_json_encode_error_resp, &result);
    }
    http_response(aio, rv, result);
    free(result);
    license_fini(&lic);
    utarray_free(plugin_names);
}

void handle_set_license(nng_aio *aio)
{
    REST_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_set_license_req_t, neu_json_decode_set_license_req, {
            NEU_JSON_RESPONSE_ERROR(set_license(req->license), {
                http_response(aio, error_code.error, result_error);
            });
        });
}
static inline bool file_exists(const char *const path)
{
    struct stat buf = { 0 };
    return -1 != stat(path, &buf);
}

static int set_license(const char *lic_str)
{
    int         rv        = 0;
    license_t   new_lic   = {};
    const char *fname_tmp = LICENSE_PATH ".tmp";

    int len = strlen(lic_str);
    if (len > 15000) {
        // the license file won't exceed 15KB
        return NEU_ERR_LICENSE_INVALID;
    }

    FILE *fp = fopen(fname_tmp, "w");
    if (NULL == fp) {
        nlog_error("unable to create license tmp file `%s`", fname_tmp);
        return NEU_ERR_EINTERNAL;
    }

    rv = fputs(lic_str, fp);

    if (rv < 0) {
        nlog_error("unable to write license to file `%s`", fname_tmp);
        rv = NEU_ERR_EINTERNAL;
        goto final;
    }

    fflush(fp);

    license_init(&new_lic);
    rv = license_read(&new_lic, fname_tmp);
    license_fini(&new_lic);
    if (0 != rv) {
        nlog_error("bad license string");
        goto final;
    }

    if (license_is_expired(&new_lic)) {
        nlog_error("expired license string");
        rv = NEU_ERR_LICENSE_EXPIRED;
        goto final;
    }

    if (file_exists(LICENSE_PATH) && 0 > backup_license_file(LICENSE_PATH)) {
        nlog_error("unable to backup license file `%s`", LICENSE_PATH);
        rv = NEU_ERR_EINTERNAL;
        goto final;
    }

    if (0 > rename(fname_tmp, LICENSE_PATH)) {
        rv = NEU_ERR_EINTERNAL;
        goto final;
    }

    neu_plugin_t *     plugin = neu_rest_get_plugin();
    neu_reqresp_head_t head   = { .type = NEU_REQ_UPDATE_LICENSE };
    rv                        = neu_plugin_op(plugin, head, NULL);
    if (0 != rv) {
        rv = NEU_ERR_EINTERNAL;
    }

final:
    fclose(fp);
    remove(fname_tmp);
    return rv;
}

int backup_license_file(const char *lic_fname)
{
    struct tm tm             = { .tm_isdst = -1 };
    time_t    now            = time(NULL);
    char      fname_bak[128] = { 0 };

    size_t n = snprintf(fname_bak, sizeof(fname_bak), "%s.bak", lic_fname);
    if (sizeof(fname_bak) == n) {
        return -1;
    }

    gmtime_r(&now, &tm);
    if (0 ==
        strftime(fname_bak + n, sizeof(fname_bak) - n, ".%Y%m%d%H%M%S", &tm)) {
        return -1;
    }

    return link(lic_fname, fname_bak);
}

int get_plugin_names(const license_t *lic, UT_array *plugin_names)
{
    for (plugin_bit_e b = PLUGIN_BIT_MODBUS_PLUS_TCP; b < PLUGIN_BIT_MAX; ++b) {
        if (license_is_plugin_enabled(lic, b)) {
            const char *s = plugin_bit_str(b);
            if (NULL == s) {
                return NEU_ERR_EINTERNAL;
            }
            utarray_push_back(plugin_names, &s);
        }
    }
    return 0;
}
