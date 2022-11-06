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
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "argparse.h"
#include "errcodes.h"
#include "parser/neu_json_license.h"
#include "plugin.h"
#include "utils/asprintf.h"
#include "utils/log.h"
#include "json/neu_json_error.h"
#include "json/neu_json_fn.h"

#include "handle.h"
#include "http.h"
#include "license.h"
#include "license_handle.h"

static int set_license(const char *lic_str);
int        backup_license_file(const char *lic_fname);
int        get_plugin_names(const license_t *lic, UT_array *plugin_names);

static inline char *get_license_path()
{
    char *s = NULL;
    neu_asprintf(&s, "persistence/%s", LICENSE_FNAME);
    return s;
}

static int copy_file(const char *from, const char *to)
{
    int         rv      = 0;
    char *      buf     = NULL;
    FILE *      fp      = NULL;
    struct stat statbuf = {};

    if (0 != stat(from, &statbuf)) {
        return NEU_ERR_EINTERNAL;
    }

    buf = malloc(statbuf.st_size);
    if (NULL == buf) {
        return NEU_ERR_EINTERNAL;
    }

    fp = fopen(from, "rb");
    if (NULL == fp) {
        rv = NEU_ERR_EINTERNAL;
        goto end;
    }

    if ((size_t) statbuf.st_size != fread(buf, 1, statbuf.st_size, fp)) {
        rv = NEU_ERR_EINTERNAL;
        goto end;
    }

    fclose(fp);
    fp = fopen(to, "w");
    if (NULL == fp) {
        rv = NEU_ERR_EINTERNAL;
        goto end;
    }

    if ((size_t) statbuf.st_size != fwrite(buf, 1, statbuf.st_size, fp)) {
        rv = NEU_ERR_EINTERNAL;
        goto end;
    }

end:
    if (fp) {
        fclose(fp);
    }
    free(buf);
    return rv;
}

int copy_license_file_if_necessary()
{
    struct stat statbuf      = {};
    char *      license_path = get_license_path();

    if (NULL == license_path) {
        return NEU_ERR_EINTERNAL;
    }

    if (0 == stat(license_path, &statbuf)) {
        free(license_path);
        return 0;
    }

    nlog_warn("license `%s` not found", license_path);

    char *fallback_path = NULL;
    neu_asprintf(&fallback_path, "%s/%s", g_config_dir, LICENSE_FNAME);
    if (NULL == fallback_path) {
        free(license_path);
        return NEU_ERR_EINTERNAL;
    }

    if (0 != stat(fallback_path, &statbuf)) {
        // no fallback license file
        free(license_path);
        free(fallback_path);
        return 0;
    }

    nlog_warn("fallback to `%s`", fallback_path);
    int rv = copy_file(fallback_path, license_path);

    free(license_path);
    free(fallback_path);
    return rv;
}

void handle_get_license(nng_aio *aio)
{
    int                         rv           = 0;
    char *                      license_path = NULL;
    license_t                   lic          = {};
    neu_json_get_license_resp_t resp         = {};
    UT_array *                  plugin_names = NULL;
    char *                      result       = NULL;

    VALIDATE_JWT(aio);

    utarray_new(plugin_names, &ut_ptr_icd);

    license_path = get_license_path();
    if (NULL == license_path) {
        rv = NEU_ERR_EINTERNAL;
        goto final;
    }

    license_init(&lic);
    rv = license_read(&lic, license_path);
    if (0 != rv) {
        goto final;
    }

    resp.license_type  = (char *) license_type_str(license_get_type(&lic));
    resp.valid         = !license_is_expired(&lic);
    resp.valid_since   = lic.since_str_;
    resp.valid_until   = lic.until_str_;
    resp.max_nodes     = license_get_max_nodes(&lic);
    resp.max_node_tags = license_get_max_node_tags(&lic);

    rv = get_plugin_names(&lic, plugin_names);
    if (0 != rv) {
        goto final;
    }
    resp.n_enabled_plugin = utarray_len(plugin_names);
    resp.enabled_plugins =
        calloc(resp.n_enabled_plugin,
               sizeof(neu_json_get_license_resp_enabled_plugin_t));
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
    free(resp.enabled_plugins);
    free(license_path);
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
    int       rv           = 0;
    size_t    len          = 0;
    char *    license_path = NULL;
    char *    fname_tmp    = NULL;
    FILE *    fp           = NULL;
    license_t new_lic      = {};

    license_path = get_license_path();
    if (NULL == license_path) {
        return NEU_ERR_EINTERNAL;
    }

    if (0 > neu_asprintf(&fname_tmp, "%s.tmp", license_path)) {
        rv = NEU_ERR_EINTERNAL;
        goto final;
    }

    len = strlen(lic_str);
    if (len > 15000) {
        // the license file won't exceed 15KB
        rv = NEU_ERR_LICENSE_INVALID;
        goto final;
    }

    fp = fopen(fname_tmp, "w");
    if (NULL == fp) {
        nlog_error("unable to create license tmp file `%s`", fname_tmp);
        rv = NEU_ERR_EINTERNAL;
        goto final;
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

    if (file_exists(license_path) && 0 > backup_license_file(license_path)) {
        nlog_error("unable to backup license file `%s`", license_path);
        rv = NEU_ERR_EINTERNAL;
        goto final;
    }

    if (0 > rename(fname_tmp, license_path)) {
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
    if (fp) {
        fclose(fp);
    }
    remove(fname_tmp);
    free(license_path);
    free(fname_tmp);
    return rv;
}

int backup_license_file(const char *lic_fname)
{
    struct tm   tm     = { .tm_isdst = -1 };
    time_t      now    = time(NULL);
    const char *suffix = ".bak";
    size_t      len =
        strlen(lic_fname) + strlen(suffix) + 15 + 1; // 15 for timestamp
    char *fname_bak = malloc(len);

    if (NULL == fname_bak) {
        return NEU_ERR_EINTERNAL;
    }

    size_t n = snprintf(fname_bak, len, "%s%s", lic_fname, suffix);

    gmtime_r(&now, &tm);
    if (0 == strftime(fname_bak + n, len - n, ".%Y%m%d%H%M%S", &tm)) {
        free(fname_bak);
        return -1;
    }

    int rv = link(lic_fname, fname_bak);
    free(fname_bak);
    return rv;
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
