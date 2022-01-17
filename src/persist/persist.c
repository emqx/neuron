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

#include <stdio.h>
#include <stdlib.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "neu_errcodes.h"
#include "vector.h"

#include "persist/json/json_adapter.h"
#include "persist/json/json_datatags.h"
#include "persist/json/json_group_configs.h"
#include "persist/json/json_plugin.h"
#include "persist/json/json_subs.h"
#include "persist/persist.h"

#include "neu_vector.h"
#include "json/neu_json_fn.h"

#if defined _WIN32 || defined __CYGWIN__
#define PATH_SEP_CHAR '\\'
#else
#define PATH_SEP_CHAR '/'
#endif

#define PATH_MAX_SIZE 128

static inline bool ends_with(const char *str, const char *suffix)
{
    size_t m = strlen(str);
    size_t n = strlen(suffix);
    return m >= n && !strncmp(str + m - n, suffix, n);
}

/**
 * Escape special characters in path string,
 *    '%' -> '%%', '/' => '%-', '.' -> '%.'
 *
 * @param buf     output buffer,
 * @param size    size of output buffer
 * @param path    path string
 *
 * @return if `buf` is NULL, return number of bytes needed to store the result.
 *         otherwise, return the number of bytes written excluding the
 *         terminating null byte, `size` indicates overflow.
 */
static int path_escape(char *buf, size_t size, const char *path)
{
    size_t i = 0;
    char   c = '\0';

    if (NULL != buf) {
        for (const char *s = path; (c = *s++) && i < size; ++i) {
            if (PATH_SEP_CHAR == c || '.' == c || '%' == c) {
                c        = (PATH_SEP_CHAR == c) ? '-' : c;
                buf[i++] = '%';
                if (size == i) {
                    break;
                }
            }
            buf[i] = c;
        }
        if (i < size) {
            buf[i] = '\0';
        } else if (i > 0) {
            buf[i - 1] = '\0';
        }
    } else {
        for (const char *s = path; (c = *s++); ++i) {
            if (PATH_SEP_CHAR == c || '.' == c || '%' == c) {
                ++i;
            }
        }
    }

    return i;
}

/**
 * Concatenate a path string to another.
 *
 * @param dst   destination path string buffer
 * @param len   destination path string len, not greater than size
 * @param size  destination path buffer size
 * @param src   path string
 *
 * @return length of the result path string excluding the terminating NULL
 *         byte, `size` indicates overflow.
 */
static int path_cat(char *dst, size_t len, size_t size, const char *src)
{
    size_t i = len;

    if (0 < i && i < size && (PATH_SEP_CHAR != dst[i - 1])) {
        dst[i++] = PATH_SEP_CHAR;
    }

    if (*src && PATH_SEP_CHAR == *src) {
        ++src;
    }

    while (i < size && (dst[i] = *src++)) {
        ++i;
    }

    if (i == size && i > 0) {
        dst[i - 1] = '\0';
    }

    return i;
}

/**
 * Escape and concatenate a path string to another.
 *
 * @param dst   destination path string buffer
 * @param len   destination path string len, not greater than size
 * @param size  destination path buffer size
 * @param src   path string
 *
 * @return length of the result path string excluding the terminating NULL
 *         byte, `size` indicates overflow.
 */
static int path_cat_escaped(char *dst, size_t len, size_t size, const char *src)
{
    size_t i = len;

    if (0 < i && i < size && (PATH_SEP_CHAR != dst[i - 1])) {
        dst[i++] = PATH_SEP_CHAR;
        dst[i]   = '\0';
    }

    size_t n = path_escape(dst + i, size - i, src);

    return i + n;
}

static inline int create_dir(char *dir_name)
{
    int rv = mkdir(dir_name, 0777);
    if (0 != rv && EEXIST == errno) {
        rv = 0;
    }
    return rv;
}

/**
 * Create a directory and any intermediate directory if necessary.
 */
static int create_dir_recursive(const char *path)
{
    int  rv               = 0;
    char p[PATH_MAX_SIZE] = { 0 };

    rv = snprintf(p, sizeof(p), "%s", path);
    if (sizeof(p) == rv) {
        return -1;
    }

    int i = 0;
    while (1) {
        while (p[i] && PATH_SEP_CHAR == p[i]) {
            ++i;
        }
        if ('\0' == p[i]) {
            break;
        }
        while (p[i] && PATH_SEP_CHAR != p[i]) {
            ++i;
        }

        char c = p[i];
        p[i]   = '\0';
        rv     = create_dir(p);
        if (0 != rv) {
            break;
        }
        p[i] = c;
    }
    return rv;
}

/**
 * Type of callback invoked in file tree walking.
 */
typedef int (*file_tree_walk_cb_t)(const char *fpath, bool is_dir, void *arg);

// depth first file tree walking implementation.
// `path_buf` is buffer for accumulating file path,
// `len` is the length of the directory path of the current invocation,
// `size` is the path buffer size.
static int file_tree_walk_(char *path_buf, size_t len, size_t size,
                           file_tree_walk_cb_t cb, void *ctx)
{
    int            rv = 0;
    DIR *          dirp;
    struct dirent *dent;

    // if not possible to read the directory for this user
    if ((dirp = opendir(path_buf)) == NULL) {
        return -1;
    }

    while (NULL != (dent = readdir(dirp))) {
        if (DT_DIR == dent->d_type) {
            // skip entries "." and ".."
            if (!strcmp(dent->d_name, ".") || !strcmp(dent->d_name, "..")) {
                continue;
            }
        } else if (DT_REG != dent->d_type) {
            continue;
        }

        // determinate a full path of an entry
        size_t n = path_cat(path_buf, len, size, dent->d_name);
        if (size == n) {
            rv = -1;
            break;
        }

        if (DT_DIR == dent->d_type) {
            rv = file_tree_walk_(path_buf, n, size, cb, ctx);
        } else {
            rv = cb(path_buf, false, ctx);
        }

        path_buf[len] = '\0'; // restore current directory path

        if (0 != rv) {
            break;
        }
    }

    if (0 == rv) {
        rv = cb(path_buf, true, ctx);
    }

    closedir(dirp);
    return rv;
}

/**
 * Depth first traversal of file tree.
 *
 * @param dir       path of directory
 * @param cb        callback to invoke for each directory entry
 * @param ctx       argument to callback
 *
 * @return  If `cb` returns nonzero, then the tree walk is terminated and the
 *          value returned by `cb` is returned as the result
 */
static int file_tree_walk(const char *dir, file_tree_walk_cb_t cb, void *ctx)
{
    char buf[PATH_MAX_SIZE] = { 0 };
    int  n                  = path_cat(buf, 0, sizeof(buf), dir);
    if (sizeof(buf) == n) {
        return -1;
    }
    return file_tree_walk_(buf, n, sizeof(buf), cb, ctx);
}

// file tree walking callback for `rmdir_recursive`
static int remove_cb(const char *fpath, bool is_dir, void *arg)
{
    (void) arg;

    if (!is_dir && !ends_with(fpath, ".json")) {
        // we are being defensive here, ignoring non-json files
        return 0;
    }
    log_info("remove %s", fpath);
    return remove(fpath);
}

/**
 * Remove directory recursively.
 * @param dir         path of directory to remove
 */
static int rmdir_recursive(const char *dir)
{
    return file_tree_walk(dir, remove_cb, NULL);
}

/**
 * Ensure that a file with the given name exists.
 *
 * @param name              file name
 * @param default_content   default file content if file not exists
 */
static inline int ensure_file_exist(const char *name,
                                    const char *default_content)
{
    int fd = open(name, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
    if (-1 == fd) {
        return NEU_ERR_FAILURE;
    }
    struct stat statbuf;
    if (-1 == fstat(fd, &statbuf)) {
        return NEU_ERR_FAILURE;
    }
    if (0 == statbuf.st_size) {
        ssize_t size = strlen(default_content);
        if (size != write(fd, default_content, size)) {
            return NEU_ERR_FAILURE;
        }
    }
    return 0;
}

static int write_file_string(const char *fn, const char *s)
{
    char tmp[PATH_MAX_SIZE] = { 0 };
    if (sizeof(tmp) == snprintf(tmp, sizeof(tmp), "%s.tmp", fn)) {
        log_error("persister too long file name:%s", fn);
        return NEU_ERR_FAILURE;
    }

    FILE *f = fopen(tmp, "w+");
    if (NULL == f) {
        log_error("persister failed to open file:%s", fn);
        return NEU_ERR_FAILURE;
    }

    // write to a temporary file first
    int n = strlen(s);
    if (((size_t) n) != fwrite(s, 1, n, f)) {
        log_error("persister failed to write file:%s", fn);
        fclose(f);
        return NEU_ERR_FAILURE;
    }

    fclose(f);

    // rename the temporary file to the destination file
    if (0 != rename(tmp, fn)) {
        log_error("persister failed rename %s to %s", tmp, fn);
        return NEU_ERR_FAILURE;
    }

    return 0;
}

// read all file contents as string
static int read_file_string(const char *fn, char **out)
{
    int fd = open(fn, O_RDONLY);
    if (-1 == fd) {
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
            goto error_read;
        }
    }

    buf[fsize] = 0;
    *out       = buf;
    close(fd);
    return 0;

error_read:
    free(buf);
error_buf:
error_fstat:
    close(fd);
error_open:
    log_error("persister fail to read %s, reason: %s", fn, strerror(errno));
    return NEU_ERR_FAILURE;
}

typedef struct neu_persister {
    const char *persist_dir;
    const char *adapters_fname;
    const char *plugins_fname;
} neu_persister_t;

neu_persister_t *neu_persister_create(char *dir_name)
{
    char dir_path[128] = { 0 };
    int  rv            = snprintf(dir_path, 128, "%s", dir_name);
    if (sizeof(dir_path) == rv) {
        log_error("dir_path exceeds maximum value");
        goto error_dir_name;
    }

    rv = create_dir(dir_path);
    if (rv != 0) {
        log_error("failed to create directory");
        goto error_dir_name;
    }

    char adapter_path[128] = { 0 };
    rv = snprintf(adapter_path, 128, "%s/adapters.json", dir_path);
    if (sizeof(adapter_path) == rv) {
        log_error("adapter_path exceeds maximun value");
        goto error_dir_name;
    }

    char plugin_path[128] = { 0 };
    rv = snprintf(plugin_path, 128, "%s/plugins.json", dir_path);
    if (sizeof(plugin_path) == rv) {
        log_error("plugin_path exceeds maximun value");
        goto error_dir_name;
    }

    neu_persister_t *persister = malloc(sizeof(neu_persister_t));
    if (persister == NULL) {
        log_error("failed to alloc memory for persister struct");
        goto error_dir_name;
    }

    if (0 != ensure_file_exist(adapter_path, "{\"nodes\": []}")) {
        log_error("persister failed to ensure file exist: %s", adapter_path);
        goto error_io;
    }
    if (0 != ensure_file_exist(plugin_path, "{\"plugins\": []}")) {
        log_error("persister failed to ensure file exist: %s", plugin_path);
        goto error_io;
    }

    persister->adapters_fname = strdup(adapter_path);
    persister->plugins_fname  = strdup(plugin_path);
    persister->persist_dir    = strdup(dir_name);

    return persister;

error_io:
    free(persister);
error_dir_name:
    return NULL;
}

static inline int persister_adapter_dir(char *buf, size_t size,
                                        neu_persister_t *persister,
                                        const char *     adapter_name)
{
    size_t n = path_cat(buf, 0, size, persister->persist_dir);
    if (size == n) {
        return n;
    }

    n = path_cat_escaped(buf, n, size, adapter_name);
    return n;
}

static inline int persister_group_configs_dir(char *buf, size_t size,
                                              neu_persister_t *persister,
                                              const char *     adapter_name)
{
    size_t n = persister_adapter_dir(buf, size, persister, adapter_name);
    if (size == n) {
        return n;
    }

    n = path_cat(buf, n, size, "groups");
    return n;
}

static inline int persister_group_config_dir(char *buf, size_t size,
                                             neu_persister_t *persister,
                                             const char *     adapter_name,
                                             const char *     group_config_name)
{
    size_t n = persister_group_configs_dir(buf, size, persister, adapter_name);
    if (size == n) {
        return n;
    }

    n = path_cat_escaped(buf, n, size, group_config_name);
    return n;
}

void neu_persister_destroy(neu_persister_t *persister)
{
    free((char *) persister->adapters_fname);
    free((char *) persister->plugins_fname);
    free((char *) persister->persist_dir);
    free(persister);
}

int neu_persister_store_adapters(neu_persister_t *persister,
                                 vector_t *       adapter_infos)
{
    neu_json_node_resp_t node_resp = {
        .n_node = adapter_infos->size,
        .nodes  = adapter_infos->data,
    };

    char *result = NULL;
    int   rv =
        neu_json_encode_by_fn(&node_resp, neu_json_encode_node_resp, &result);
    if (rv != 0) {
        return rv;
    }

    rv = write_file_string(persister->adapters_fname, result);

    free(result);
    return rv;
}

int neu_persister_load_adapters(neu_persister_t *persister,
                                vector_t **      adapter_infos)
{
    char *json_str = NULL;
    int   rv       = read_file_string(persister->adapters_fname, &json_str);
    if (rv != 0) {
        return rv;
    }

    neu_json_node_req_t *node_req = NULL;
    rv = neu_json_decode_node_req(json_str, &node_req);
    free(json_str);
    if (rv != 0) {
        return rv;
    }

    rv            = 0;
    vector_t *vec = vector_new_move_from_buf(
        node_req->nodes, node_req->n_node, node_req->n_node,
        sizeof(neu_persist_adapter_info_t));
    if (vec == NULL) {
        neu_json_decode_node_req_free(node_req);
        rv = -1;
        goto load_adapters_exit;
    }

    *adapter_infos   = vec;
    node_req->n_node = 0;
    node_req->nodes  = NULL;

load_adapters_exit:
    neu_json_decode_node_req_free(node_req);
    return rv;
}

int neu_persister_delete_adapter(neu_persister_t *persister,
                                 const char *     adapter_name)
{
    (void *) persister;
    (void *) adapter_name;
    return 0;
}

int neu_persister_store_plugins(neu_persister_t *persister,
                                vector_t *       plugin_infos)
{
    neu_json_plugin_resp_t plugin_resp = {
        .n_plugin = plugin_infos->size,
        .plugins  = plugin_infos->data,
    };

    char *result = NULL;
    int   rv = neu_json_encode_by_fn(&plugin_resp, neu_json_encode_plugin_resp,
                                   &result);
    if (rv != 0) {
        return rv;
    }

    rv = write_file_string(persister->plugins_fname, result);

    free(result);
    return rv;
}

int neu_persister_load_plugins(neu_persister_t *persister,
                               vector_t **      plugin_infos)
{
    char *json_str = NULL;
    int   rv       = read_file_string(persister->plugins_fname, &json_str);
    if (rv != 0) {
        return rv;
    }

    neu_json_plugin_req_t *plugin_req = NULL;
    rv = neu_json_decode_plugin_req(json_str, &plugin_req);
    free(json_str);
    if (rv != 0) {
        return rv;
    }

    rv            = 0;
    vector_t *vec = vector_new_move_from_buf(
        plugin_req->plugins, plugin_req->n_plugin, plugin_req->n_plugin,
        sizeof(neu_persist_plugin_info_t));
    if (vec == NULL) {
        rv = -1;
        goto load_plugins_exit;
    }

    *plugin_infos        = vec;
    plugin_req->n_plugin = 0;
    plugin_req->plugins  = NULL;

load_plugins_exit:
    neu_json_decode_plugin_req_free(plugin_req);
    return rv;
}

int neu_persister_store_datatags(neu_persister_t *persister,
                                 const char *     adapter_name,
                                 vector_t *       datatag_infos)
{
    char datatag_path[128] = { 0 };

    int rv = snprintf(datatag_path, 128, "%s/persist/%s",
                      persister->persist_dir, adapter_name);
    if (sizeof(datatag_path) == rv) {
        log_error("datatag_path exceeds maximum value");
        return -1;
    }

    create_dir(datatag_path);

    char datatag_file[128] = { 0 };

    rv = snprintf(datatag_file, 128, "%s/datatags.json", datatag_path);
    if (sizeof(datatag_file) == rv) {
        log_error("datatag_file exceeds maximum value");
        return -1;
    }

    neu_json_datatag_req_t datatags_resp = {
        .n_tag = datatag_infos->size,
        .tags  = datatag_infos->data,
    };

    char *result = NULL;
    rv = neu_json_encode_by_fn(&datatags_resp, neu_json_encode_datatag_resp,
                               &result);
    if (rv != 0) {
        free(result);
        return rv;
    }

    rv = write_file_string(datatag_file, result);

    free(result);
    return rv;
}

int neu_persister_load_datatags(neu_persister_t *persister,
                                const char *     adapter_name,
                                vector_t **      datatag_infos)
{
    char *datatags          = NULL;
    char  datatag_file[128] = { 0 };

    int rv = snprintf(datatag_file, 128, "%s/persist/%s/datatags.json",
                      persister->persist_dir, adapter_name);
    if (sizeof(datatag_file) == rv) {
        log_error("datatg_file exceeds maximum value");
        return -1;
    }

    rv = read_file_string(datatag_file, &datatags);
    if (rv != 0) {
        return rv;
    }

    neu_json_datatag_req_t *datatag_req = NULL;
    rv = neu_json_decode_datatag_req(datatags, &datatag_req);
    if (rv != 0) {
        return rv;
    }

    vector_t *vec = vector_new_move_from_buf(
        datatag_req->tags, datatag_req->n_tag, datatag_req->n_tag,
        sizeof(neu_persist_datatag_info_t));
    if (vec == NULL) {
        return -1;
    }

    *datatag_infos     = vec;
    datatag_req->n_tag = 0;
    datatag_req->tags  = NULL;

    neu_json_decode_datatag_req_free(datatag_req);
    return 0;
}

int neu_persister_store_subscriptions(neu_persister_t *persister,
                                      const char *     adapter_name,
                                      vector_t *       subscription_infos)
{
    char subs_path[128] = { 0 };

    int rv = snprintf(subs_path, 128, "%s/persist/%s", persister->persist_dir,
                      adapter_name);
    if (sizeof(subs_path) == rv) {
        log_error("subs_path exceeds maximum value");
        return -1;
    }
    create_dir(subs_path);

    char subs_file[128] = { 0 };
    rv = snprintf(subs_file, 128, "%s/subscriptions.json", subs_path);
    if (sizeof(subs_file) == rv) {
        log_error("subs_file exceeds maximum value");
        return -1;
    }

    neu_json_subscriptions_req_t subs_resp = {
        .n_subscription = subscription_infos->size,
        .subscriptions  = subscription_infos->data,
    };

    char *result = NULL;
    rv = neu_json_encode_by_fn(&subs_resp, neu_json_encode_subscriptions_resp,
                               &result);
    if (rv != 0) {
        free(result);
        return rv;
    }

    rv = write_file_string(subs_file, result);

    free(result);
    return rv;
}

int neu_persister_load_subscriptions(neu_persister_t *persister,
                                     const char *     adapter_name,
                                     vector_t **      subscription_infos)
{
    char *subs = NULL;

    char subs_file[128] = { 0 };
    int  rv = snprintf(subs_file, 128, "%s/persist/%s/subscriptions.json",
                      persister->persist_dir, adapter_name);
    if (sizeof(subs_file) == rv) {
        log_error("subs_file exceeds maximum value");
        return -1;
    }

    rv = read_file_string(subs_file, &subs);
    if (rv != 0) {
        return rv;
    }

    neu_json_subscriptions_req_t *subs_req = NULL;
    rv = neu_json_decode_subscriptions_req(subs, &subs_req);
    if (rv != 0) {
        return rv;
    }

    vector_t *vec = vector_new_move_from_buf(
        subs_req->subscriptions, subs_req->n_subscription,
        subs_req->n_subscription, sizeof(neu_persist_subscription_info_t));

    if (vec == NULL) {
        return -1;
    }

    *subscription_infos      = vec;
    subs_req->n_subscription = 0;
    subs_req->subscriptions  = NULL;

    neu_json_decode_subscriptions_req_free(subs_req);
    return 0;
}

int neu_persister_store_group_config(
    neu_persister_t *persister, const char *adapter_name,
    neu_persist_group_config_info_t *group_config_info)
{
    char path[PATH_MAX_SIZE] = { 0 };

    int n =
        persister_group_config_dir(path, sizeof(path), persister, adapter_name,
                                   group_config_info->group_config_name);
    if (sizeof(path) == n) {
        log_error("persister path too long: %s", path);
        return -1;
    }

    int rv = create_dir_recursive(path);
    if (0 != rv) {
        log_error("persister failed to create dir: %s", path);
        return rv;
    }

    n = path_cat(path, n, sizeof(path), "config.json");
    if (sizeof(path) == n) {
        log_error("persister path too long: %s", path);
        return -1;
    }

    char *result = NULL;
    rv           = neu_json_encode_by_fn(group_config_info,
                               neu_json_encode_group_configs_resp, &result);
    if (rv == 0) {
        rv = write_file_string(path, result);
    }

    free(result);
    return rv;
}

int neu_persister_load_group_configs(neu_persister_t *persister,
                                     const char *     adapter_name,
                                     const char *     group_config_name,
                                     vector_t **      group_config_infos)
{
    char *group_configs = NULL;

    char group_config_file[128] = { 0 };
    int  rv = snprintf(group_config_file, 128, "%s/persist/%s/group_configs/%s",
                      persister->persist_dir, adapter_name, group_config_name);
    if (sizeof(group_config_file) == rv) {
        log_error("group_config_file exceeds maximum value");
        return -1;
    }

    rv = read_file_string(group_config_file, &group_configs);
    if (rv != 0) {
        return rv;
    }

    neu_json_group_configs_req_t *group_config_req = NULL;
    rv = neu_json_decode_group_configs_req(group_configs, &group_config_req);
    if (rv != 0) {
        return rv;
    }

    // vector_t *vec = vector_new_move_from_buf(
    //     group_config_req->datatag_names, group_config_req->datatag_names,
    //     group_config_req->group_config_name,
    //     group_config_req->n_datatag_name, group_config_req->read_interval,
    //     sizeof(neu_persist_group_config_info_t));
    // if (vec == NULL) {
    //     return -1;
    // }

    // *group_config_infos                 = vec;
    // group_config_req->adapter_name      = NULL;
    // group_config_req->datatag_names     = NULL;
    // group_config_req->group_config_name = NULL;
    // group_config_req->n_datatag_name    = 0;
    // group_config_req->read_interval     = 0;

    neu_json_decode_group_configs_req_free(group_config_req);
    return 0;
}

int neu_persister_delete_group_config(neu_persister_t *persister,
                                      const char *     adapter_name,
                                      const char *     group_config_name)
{
    char path[PATH_MAX_SIZE] = { 0 };

    int n = persister_group_config_dir(path, sizeof(path), persister,
                                       adapter_name, group_config_name);
    if (sizeof(path) == n) {
        log_error("persister path too long: %s", path);
        return -1;
    }

    int rv = rmdir_recursive(path);
    return rv;
}

int neu_persister_store_adapter_setting(neu_persister_t *persister,
                                        const char *     adapter_name,
                                        const char *     setting)
{
    char path[PATH_MAX_SIZE] = { 0 };

    int n = persister_adapter_dir(path, sizeof(path), persister, adapter_name);
    if (sizeof(path) == n) {
        log_error("persister path too long: %s", path);
        return -1;
    }
    if (0 != create_dir(path)) {
        log_error("persister failed to create dir: %s", path);
        return -1;
    }

    n = path_cat(path, n, sizeof(path), "settings.json");
    if (sizeof(path) == n) {
        log_error("persister path too long: %s", path);
        return -1;
    }

    int rv = write_file_string(path, setting);

    return rv;
}

int neu_persister_load_adapter_setting(neu_persister_t *  persister,
                                       const char *       adapter_name,
                                       const char **const setting)
{
    char path[PATH_MAX_SIZE] = { 0 };

    int n = persister_adapter_dir(path, sizeof(path), persister, adapter_name);
    if (sizeof(path) == n) {
        log_error("persister path too long: %s", path);
        return -1;
    }

    n = path_cat(path, n, sizeof(path), "settings.json");
    if (sizeof(path) == n) {
        log_error("persister path too long: %s", path);
        return -1;
    }

    int rv = read_file_string(path, (char **) setting);

    return rv;
}

int neu_persister_delete_adapter_setting(neu_persister_t *persister,
                                         const char *     adapter_name)
{
    (void *) persister;
    (void *) adapter_name;
    return 0;
}
