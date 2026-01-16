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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "errcodes.h"
#include "utils/asprintf.h"
#include "utils/log.h"
#include "utils/time.h"

#include "argparse.h"
#include "persist/json/persist_json_plugin.h"
#include "persist/persist.h"
#include "persist/persist_impl.h"
#include "persist/sqlite.h"

#include "json/neu_json_fn.h"

#if defined _WIN32 || defined __CYGWIN__
#define PATH_SEP_CHAR '\\'
#else
#define PATH_SEP_CHAR '/'
#endif

#define PATH_MAX_SIZE 128

static const char *     plugin_file = "persistence/plugins.json";
static const char *     tmp_path    = "tmp";
static neu_persister_t *g_impl      = NULL;

static int write_file_string(const char *fn, const char *s)
{
    char *tmp = NULL;
    if (0 > neu_asprintf(&tmp, "%s.tmp", fn)) {
        nlog_error("persister too long file name:%s", fn);
        return -1;
    }

    FILE *f = fopen(tmp, "w+");
    if (NULL == f) {
        nlog_error("persister failed to open file:%s", fn);
        free(tmp);
        return -1;
    }

    // write to a temporary file first
    int n = strlen(s);
    if (((size_t) n) != fwrite(s, 1, n, f)) {
        nlog_error("persister failed to write file:%s", fn);
        fclose(f);
        free(tmp);
        return -1;
    }

    fclose(f);

    nlog_debug("persister write %s to %s", s, tmp);

    // rename the temporary file to the destination file
    if (0 != rename(tmp, fn)) {
        nlog_error("persister failed rename %s to %s", tmp, fn);
        free(tmp);
        return -1;
    }

    free(tmp);
    return 0;
}

int read_file_string(const char *fn, char **out)
{
    int rv = 0;
    int fd = open(fn, O_RDONLY);
    if (-1 == fd) {
        rv = -1;
        goto error_open;
    }

    // get file size
    struct stat statbuf;
    if (-1 == fstat(fd, &statbuf)) {
        goto error_fstat;
    }
    off_t fsize = statbuf.st_size;

    char *buf = malloc(fsize + 1);
    if (NULL == buf) {
        rv = -1;
        goto error_buf;
    }

    // read all file content
    ssize_t n = 0;
    while ((n = read(fd, buf + n, fsize))) {
        if (fsize == n) {
            break;
        } else if (n < fsize && EINTR == errno) {
            continue;
        } else {
            rv = -1;
            goto error_read;
        }
    }

    buf[fsize] = 0;
    *out       = buf;
    close(fd);
    return rv;

error_read:
    free(buf);
error_buf:
error_fstat:
    close(fd);
error_open:
    nlog_warn("persister fail to read %s, reason: %s", fn, strerror(errno));
    return rv;
}

int neu_persister_store_plugins(UT_array *plugin_infos)
{
    int                    index       = 0;
    neu_json_plugin_resp_t plugin_resp = {
        .n_plugin = utarray_len(plugin_infos),
    };

    plugin_resp.plugins = calloc(utarray_len(plugin_infos), sizeof(char *));

    utarray_foreach(plugin_infos, neu_resp_plugin_info_t *, plugin)
    {
        // if (NEU_PLUGIN_KIND_SYSTEM == plugin->kind) {
        //     // filter out system plugins
        //     continue;
        // }
        plugin_resp.plugins[index] = plugin->library;
        index++;
    }

    char *result = NULL;
    int   rv = neu_json_encode_by_fn(&plugin_resp, neu_json_encode_plugin_resp,
                                   &result);

    free(plugin_resp.plugins);
    if (rv != 0) {
        return rv;
    }

    rv = write_file_string(plugin_file, result);

    free(result);
    return rv;
}

static int load_plugins_file(const char *fname, UT_array *plugin_infos)
{
    char *json_str = NULL;
    int   rv       = read_file_string(fname, &json_str);
    if (rv != 0) {
        return rv;
    }

    neu_json_plugin_req_t *plugin_req = NULL;
    rv = neu_json_decode_plugin_req(json_str, &plugin_req);
    if (rv != 0) {
        free(json_str);
        return rv;
    }

    for (int i = 0; i < plugin_req->n_plugin; i++) {
        char *name = plugin_req->plugins[i];
        utarray_push_back(plugin_infos, &name);
    }

    free(json_str);
    free(plugin_req->plugins);
    free(plugin_req);
    return 0;
}

static int ut_str_cmp(const void *a, const void *b)
{
    return strcmp(*(char **) a, *(char **) b);
}

int neu_persister_load_plugins(UT_array **plugin_infos)
{
    UT_array *default_plugins = NULL;
    UT_array *user_plugins    = NULL;
    utarray_new(default_plugins, &ut_ptr_icd);
    utarray_new(user_plugins, &ut_ptr_icd);

    // default plugins will always present
    if (0 !=
        load_plugins_file("config/default_plugins.json", default_plugins)) {
        nlog_warn("cannot load default plugins");
    }
    // user plugins
    if (0 != load_plugins_file(plugin_file, user_plugins)) {
        nlog_warn("cannot load user plugins");
    } else {
        // the following operation needs sorting
        utarray_sort(default_plugins, ut_str_cmp);
    }

    utarray_foreach(user_plugins, char **, name)
    {
        // filter out duplicates in case of old persistence data
        char **find = utarray_find(default_plugins, name, ut_str_cmp);
        if (NULL == find) {
            utarray_push_back(default_plugins, name);
            *name = NULL; // move to default_plugins
        } else {
            free(*name);
        }
    }

    utarray_free(user_plugins);
    *plugin_infos = default_plugins;
    return 0;
}

char *neu_persister_save_file_tmp(const char *file_data, uint32_t len,
                                  const char *suffix)
{
    struct stat st;

    if (stat(tmp_path, &st) == -1) {
        if (mkdir(tmp_path, 0700) == -1) {
            nlog_error("%s mkdir fail", tmp_path);
            return NULL;
        }
    }

    char *file_name = calloc(1, 128);
    snprintf(file_name, 128, "%s/%" PRId64 ".%s", tmp_path, neu_time_ms(),
             suffix);
    FILE *fp = NULL;
    fp       = fopen(file_name, "wb+");
    if (fp == NULL) {
        nlog_error("not create tmp file: %s, err:%s", file_name,
                   strerror(errno));
        free(file_name);
        return NULL;
    }

    size_t size = fwrite(file_data, 1, len, fp);

    if (size < len) {
        fclose(fp);
        free(file_name);
        return NULL;
    }

    fclose(fp);

    return file_name;
}

bool neu_persister_library_exists(const char *library)
{
    bool      ret          = false;
    UT_array *plugin_infos = NULL;

    int rv = neu_persister_load_plugins(&plugin_infos);
    if (rv != 0) {
        return ret;
    }

    utarray_foreach(plugin_infos, char **, name)
    {
        if (strcmp(library, *name) == 0) {
            ret = true;
            break;
        }
    }

    utarray_foreach(plugin_infos, char **, name) { free(*name); }
    utarray_free(plugin_infos);

    return ret;
}

int neu_persister_create(const char *schema_dir)
{
    g_impl = neu_sqlite_persister_create(schema_dir);
    if (NULL == g_impl) {
        return -1;
    }
    return 0;
}

sqlite3 *neu_persister_get_db()
{
    return g_impl->vtbl->native_handle(g_impl);
}

void neu_persister_destroy()
{
    g_impl->vtbl->destroy(g_impl);
}

int neu_persister_store_node(neu_persist_node_info_t *info)
{
    return g_impl->vtbl->store_node(g_impl, info);
}

int neu_persister_load_nodes(UT_array **node_infos)
{
    return g_impl->vtbl->load_nodes(g_impl, node_infos);
}

int neu_persister_delete_node(const char *node_name)
{
    return g_impl->vtbl->delete_node(g_impl, node_name);
}

int neu_persister_update_node(const char *node_name, const char *new_name)
{
    return g_impl->vtbl->update_node(g_impl, node_name, new_name);
}

int neu_persister_update_node_tags(const char *node_name, const char *tags)
{
    return g_impl->vtbl->update_node_tags(g_impl, node_name, tags);
}

int neu_persister_update_node_state(const char *node_name, int state)
{
    return g_impl->vtbl->update_node_state(g_impl, node_name, state);
}

int neu_persister_store_tag(const char *driver_name, const char *group_name,
                            const neu_datatag_t *tag)
{
    return g_impl->vtbl->store_tag(g_impl, driver_name, group_name, tag);
}

int neu_persister_store_tags(const char *driver_name, const char *group_name,
                             const neu_datatag_t *tags, size_t n)
{
    return g_impl->vtbl->store_tags(g_impl, driver_name, group_name, tags, n);
}

int neu_persister_load_tags(const char *driver_name, const char *group_name,
                            UT_array **tags)
{
    return g_impl->vtbl->load_tags(g_impl, driver_name, group_name, tags);
}

int neu_persister_update_tag(const char *driver_name, const char *group_name,
                             const neu_datatag_t *tag)
{
    return g_impl->vtbl->update_tag(g_impl, driver_name, group_name, tag);
}

int neu_persister_update_tag_value(const char *         driver_name,
                                   const char *         group_name,
                                   const neu_datatag_t *tag)
{
    return g_impl->vtbl->update_tag_value(g_impl, driver_name, group_name, tag);
}

int neu_persister_delete_tag(const char *driver_name, const char *group_name,
                             const char *tag_name)
{
    return g_impl->vtbl->delete_tag(g_impl, driver_name, group_name, tag_name);
}

int neu_persister_store_subscription(const char *app_name,
                                     const char *driver_name,
                                     const char *group_name, const char *params,
                                     const char *static_tags)
{
    return g_impl->vtbl->store_subscription(g_impl, app_name, driver_name,
                                            group_name, params, static_tags);
}

int neu_persister_update_subscription(const char *app_name,
                                      const char *driver_name,
                                      const char *group_name,
                                      const char *params,
                                      const char *static_tags)
{
    return g_impl->vtbl->update_subscription(g_impl, app_name, driver_name,
                                             group_name, params, static_tags);
}

int neu_persister_load_subscriptions(const char *app_name,
                                     UT_array ** subscription_infos)
{
    return g_impl->vtbl->load_subscriptions(g_impl, app_name,
                                            subscription_infos);
}

int neu_persister_delete_subscription(const char *app_name,
                                      const char *driver_name,
                                      const char *group_name)
{
    return g_impl->vtbl->delete_subscription(g_impl, app_name, driver_name,
                                             group_name);
}

int neu_persister_store_group(const char *              driver_name,
                              neu_persist_group_info_t *group_info,
                              const char *              context)
{
    return g_impl->vtbl->store_group(g_impl, driver_name, group_info, context);
}

int neu_persister_update_group(const char *driver_name, const char *group_name,
                               neu_persist_group_info_t *group_info)
{
    return g_impl->vtbl->update_group(g_impl, driver_name, group_name,
                                      group_info);
}

int neu_persister_load_groups(const char *driver_name, UT_array **group_infos)
{
    return g_impl->vtbl->load_groups(g_impl, driver_name, group_infos);
}

int neu_persister_delete_group(const char *driver_name, const char *group_name)
{
    return g_impl->vtbl->delete_group(g_impl, driver_name, group_name);
}

int neu_persister_store_node_setting(const char *node_name, const char *setting)
{
    return g_impl->vtbl->store_node_setting(g_impl, node_name, setting);
}

int neu_persister_load_node_setting(const char *       node_name,
                                    const char **const setting)
{
    return g_impl->vtbl->load_node_setting(g_impl, node_name, setting);
}

int neu_persister_delete_node_setting(const char *node_name)
{
    return g_impl->vtbl->delete_node_setting(g_impl, node_name);
}

int neu_persister_store_user(const neu_persist_user_info_t *user)
{
    return g_impl->vtbl->store_user(g_impl, user);
}

int neu_persister_update_user(const neu_persist_user_info_t *user)
{
    return g_impl->vtbl->update_user(g_impl, user);
}

int neu_persister_load_user(const char *              user_name,
                            neu_persist_user_info_t **user_p)
{
    return g_impl->vtbl->load_user(g_impl, user_name, user_p);
}

int neu_persister_delete_user(const char *user_name)
{
    return g_impl->vtbl->delete_user(g_impl, user_name);
}

int neu_persister_load_users(UT_array **user_infos)
{
    return g_impl->vtbl->load_users(g_impl, user_infos);
}

int neu_persister_store_server_cert(
    const neu_persist_server_cert_info_t *cert_info)
{
    return g_impl->vtbl->store_server_cert(g_impl, cert_info);
}

int neu_persister_update_server_cert(
    const neu_persist_server_cert_info_t *cert_info)
{
    return g_impl->vtbl->update_server_cert(g_impl, cert_info);
}

int neu_persister_load_server_cert(const char *                     app_name,
                                   neu_persist_server_cert_info_t **cert_info_p)
{
    return g_impl->vtbl->load_server_cert(g_impl, app_name, cert_info_p);
}

int neu_persister_store_client_cert(
    const neu_persist_client_cert_info_t *cert_info)
{
    return g_impl->vtbl->store_client_cert(g_impl, cert_info);
}

int neu_persister_update_client_cert(
    const neu_persist_client_cert_info_t *cert_info)
{
    return g_impl->vtbl->update_client_cert(g_impl, cert_info);
}

int neu_persister_load_client_certs_by_app(const char *app_name,
                                           UT_array ** cert_infos)
{
    return g_impl->vtbl->load_client_certs_by_app(g_impl, app_name, cert_infos);
}

int neu_persister_load_client_certs(UT_array **cert_infos)
{
    return g_impl->vtbl->load_client_certs(g_impl, cert_infos);
}

int neu_persister_delete_client_cert(const char *fingerprint)
{
    return g_impl->vtbl->delete_client_cert(g_impl, fingerprint);
}

int neu_persister_store_security_policy(
    const neu_persist_security_policy_info_t *policy_info)
{
    return g_impl->vtbl->store_security_policy(g_impl, policy_info);
}

int neu_persister_update_security_policy(
    const neu_persist_security_policy_info_t *policy_info)
{
    return g_impl->vtbl->update_security_policy(g_impl, policy_info);
}

int neu_persister_load_security_policy(
    const char *app_name, neu_persist_security_policy_info_t **policy_info_p)
{
    return g_impl->vtbl->load_security_policy(g_impl, app_name, policy_info_p);
}

int neu_persister_load_security_policies(UT_array **policy_infos)
{
    return g_impl->vtbl->load_security_policies(g_impl, policy_infos);
}

int neu_persister_store_auth_setting(
    const neu_persist_auth_setting_info_t *auth_info)
{
    return g_impl->vtbl->store_auth_setting(g_impl, auth_info);
}

int neu_persister_update_auth_setting(
    const neu_persist_auth_setting_info_t *auth_info)
{
    return g_impl->vtbl->update_auth_setting(g_impl, auth_info);
}

int neu_persister_load_auth_setting(
    const char *app_name, neu_persist_auth_setting_info_t **auth_info_p)
{
    return g_impl->vtbl->load_auth_setting(g_impl, app_name, auth_info_p);
}

int neu_persister_store_auth_user(const neu_persist_auth_user_info_t *user_info)
{
    return g_impl->vtbl->store_auth_user(g_impl, user_info);
}

int neu_persister_update_auth_user(
    const neu_persist_auth_user_info_t *user_info)
{
    return g_impl->vtbl->update_auth_user(g_impl, user_info);
}

int neu_persister_load_auth_user(const char *app_name, const char *username,
                                 neu_persist_auth_user_info_t **user_info_p)
{
    return g_impl->vtbl->load_auth_user(g_impl, app_name, username,
                                        user_info_p);
}

int neu_persister_load_auth_users_by_app(const char *app_name,
                                         UT_array ** user_infos)
{
    return g_impl->vtbl->load_auth_users_by_app(g_impl, app_name, user_infos);
}

int neu_persister_delete_auth_user(const char *app_name, const char *username)
{
    return g_impl->vtbl->delete_auth_user(g_impl, app_name, username);
}
