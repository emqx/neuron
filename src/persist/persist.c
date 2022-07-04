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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "errcodes.h"
#include "utils/log.h"

#include "persist/json/persist_json_adapter.h"
#include "persist/json/persist_json_datatags.h"
#include "persist/json/persist_json_group_configs.h"
#include "persist/json/persist_json_plugin.h"
#include "persist/json/persist_json_subs.h"
#include "persist/persist.h"

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
    return m >= n && !strcmp(str + m - n, suffix);
}

typedef struct neu_persister {
    const char *persist_dir;
    const char *adapters_fname;
    const char *plugins_fname;
} neu_persister_t;

const char *neu_persister_get_persist_dir(neu_persister_t *persister)
{
    if (persister == NULL) {
        return NULL;
    }

    return persister->persist_dir;
}

const char *neu_persister_get_adapters_fname(neu_persister_t *persister)
{
    if (persister == NULL) {
        return NULL;
    }

    return persister->adapters_fname;
}

const char *neu_persister_get_plugins_fname(neu_persister_t *persister)
{
    if (persister == NULL) {
        return NULL;
    }

    return persister->plugins_fname;
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
    char   c;

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
        rv = -1;
        return rv;
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
    nlog_info("remove %s", fpath);
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
        return -1;
    }
    struct stat statbuf;
    if (-1 == fstat(fd, &statbuf)) {
        return -1;
    }
    if (0 == statbuf.st_size) {
        ssize_t size = strlen(default_content);
        if (size != write(fd, default_content, size)) {
            return -1;
        }
    }
    return 0;
}

static int write_file_string(const char *fn, const char *s)
{
    char tmp[PATH_MAX_SIZE] = { 0 };
    if (sizeof(tmp) == snprintf(tmp, sizeof(tmp), "%s.tmp", fn)) {
        nlog_error("persister too long file name:%s", fn);
        return -1;
    }

    FILE *f = fopen(tmp, "w+");
    if (NULL == f) {
        nlog_error("persister failed to open file:%s", fn);
        return -1;
    }

    // write to a temporary file first
    int n = strlen(s);
    if (((size_t) n) != fwrite(s, 1, n, f)) {
        nlog_error("persister failed to write file:%s", fn);
        fclose(f);
        return -1;
    }

    fclose(f);

    nlog_debug("persister write %s to %s", s, tmp);

    // rename the temporary file to the destination file
    if (0 != rename(tmp, fn)) {
        nlog_error("persister failed rename %s to %s", tmp, fn);
        return -1;
    }

    return 0;
}

// read all file contents as string
static int read_file_string(const char *fn, char **out)
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

// file tree walking callback for collecting adapter infos
static int read_adapter_cb(const char *fpath, bool is_dir, void *arg)
{
    UT_array *adapter_infos = arg;
    int       rv            = 0;

    if (is_dir) {
        return 0;
    }

    if (!ends_with(fpath, "adapter.json")) {
        return 0;
    }

    char *json_str = NULL;
    rv             = read_file_string(fpath, &json_str);
    if (0 != rv) {
        return rv;
    }

    neu_json_node_req_node_t *node = NULL;
    rv = neu_json_decode_node_req_node(json_str, &node);
    free(json_str);
    if (0 != rv) {
        return 0; // ignore bad adapter data
    }

    nlog_info("read %s", fpath);
    utarray_push_back(adapter_infos, node);
    free(node);
    return rv;
}

// file tree walking callback for collecting adapter group config infos
static int read_group_config_cb(const char *fpath, bool is_dir, void *arg)
{
    UT_array *group_config_infos = arg;
    int       rv                 = 0;

    if (is_dir) {
        return 0;
    }

    if (!ends_with(fpath, "config.json")) {
        return 0;
    }

    char *json_str = NULL;
    rv             = read_file_string(fpath, &json_str);
    if (0 != rv) {
        return rv;
    }

    neu_json_group_configs_req_t *group_config_req = NULL;
    rv = neu_json_decode_group_configs_req(json_str, &group_config_req);
    free(json_str);
    if (0 != rv) {
        return 0; // ignore bad group config data
    }

    nlog_info("read %s", fpath);
    utarray_push_back(group_config_infos, group_config_req);
    free(group_config_req);

    return rv;
}

neu_persister_t *neu_persister_create(const char *dir_name)
{
    int   rv                  = 0;
    char *persist_dir         = NULL;
    char *adapters_fname      = NULL;
    char *plugins_fname       = NULL;
    char  path[PATH_MAX_SIZE] = { 0 };

    int dir_len = path_cat(path, 0, sizeof(path), dir_name);
    if (sizeof(path) == dir_len) {
        nlog_error("path too long: %s", dir_name);
        goto error;
    }
    persist_dir = strdup(dir_name);
    if (NULL == persist_dir) {
        nlog_error("fail to strdup: %s", dir_name);
        goto error;
    }

    rv = create_dir(persist_dir);
    if (rv != 0) {
        nlog_error("failed to create directory: %s", persist_dir);
        goto error;
    }

    int n = path_cat(path, dir_len, sizeof(path), "adapters");
    if (sizeof(path) == n) {
        nlog_error("path too long: %s", path);
        goto error;
    }
    rv = create_dir(path);
    if (rv != 0) {
        nlog_error("failed to create directory: %s", path);
        goto error;
    }

    n = path_cat(path, dir_len, sizeof(path), "adapters.json");
    if (sizeof(path) == n) {
        nlog_error("path too long: %s", path);
        goto error;
    }
    adapters_fname = strdup(path);
    if (NULL == adapters_fname) {
        nlog_error("fail to strdup: %s", path);
        goto error;
    }

    n = path_cat(path, dir_len, sizeof(path), "plugins.json");
    if (sizeof(path) == n) {
        nlog_error("path too long: %s", path);
        goto error;
    }
    plugins_fname = strdup(path);
    if (NULL == plugins_fname) {
        nlog_error("fail to strdup: %s", path);
        goto error;
    }

    if (0 != ensure_file_exist(plugins_fname, "{\"plugins\": []}")) {
        nlog_error("persister failed to ensure file exist: %s", plugins_fname);
        goto error;
    }

    neu_persister_t *persister = malloc(sizeof(neu_persister_t));
    if (NULL == persister) {
        nlog_error("failed to alloc memory for persister struct");
        goto error;
    }

    persister->plugins_fname  = plugins_fname;
    persister->adapters_fname = adapters_fname;
    persister->persist_dir    = persist_dir;

    return persister;

error:
    free(plugins_fname);
    free(adapters_fname);
    free(persist_dir);
    return NULL;
}

static inline int persister_adapters_dir(char *buf, size_t size,
                                         neu_persister_t *persister)
{
    size_t n = path_cat(buf, 0, size, persister->persist_dir);
    if (size == n) {
        return n;
    }

    n = path_cat(buf, n, size, "adapters");
    return n;
}

static inline int persister_adapter_dir(char *buf, size_t size,
                                        neu_persister_t *persister,
                                        const char *     adapter_name)
{
    size_t n = persister_adapters_dir(buf, size, persister);
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

static int migrate_adapters_file(neu_persister_t *persister)
{
    char *json_str = NULL;
    int   rv       = read_file_string(persister->adapters_fname, &json_str);
    if (rv != 0) {
        return 0;
    }

    neu_json_node_req_t *node_req = NULL;
    rv = neu_json_decode_node_req(json_str, &node_req);
    free(json_str);
    json_str = NULL;
    if (rv != 0) {
        nlog_error("decode adapters json file fail");
        return rv;
    }

    bool all_migrated = true;
    for (int i = 0; i < node_req->n_node; ++i) {
        neu_json_node_req_node_t *node = &node_req->nodes[i];
        rv = neu_persister_store_adapter(persister, node);
        if (0 != rv) {
            nlog_error("migrate adapter %s fail", node->name);
            all_migrated = false;
        }
    }

    if (all_migrated) {
        nlog_info("remove %s", persister->adapters_fname);
        remove(persister->adapters_fname);
    }

    neu_json_decode_node_req_free(node_req);
    return rv;
}

int neu_persister_store_adapter(neu_persister_t *           persister,
                                neu_persist_adapter_info_t *info)
{
    char path[PATH_MAX_SIZE] = { 0 };

    int n = persister_adapter_dir(path, sizeof(path), persister, info->name);
    if (sizeof(path) == n) {
        nlog_error("persister path too long: %s", path);
        return -1;
    }
    if (0 != create_dir_recursive(path)) {
        nlog_error("persister failed to create dir: %s", path);
        return -1;
    }

    n = path_cat(path, n, sizeof(path), "adapter.json");
    if (sizeof(path) == n) {
        nlog_error("persister path too long: %s", path);
        return -1;
    }

    char *result = NULL;
    int   rv =
        neu_json_encode_by_fn(info, neu_json_encode_node_resp_node, &result);
    if (rv != 0) {
        return rv;
    }

    rv = write_file_string(path, result);

    free(result);
    return rv;
}

int neu_persister_load_adapters(neu_persister_t *persister,
                                UT_array **      adapter_infos)
{
    char   path[PATH_MAX_SIZE] = { 0 };
    UT_icd icd = { sizeof(neu_persist_adapter_info_t), NULL, NULL, NULL };

    migrate_adapters_file(persister);

    if (sizeof(path) == persister_adapters_dir(path, sizeof(path), persister)) {
        return -1;
    }

    utarray_new(*adapter_infos, &icd);

    int rv = file_tree_walk(path, read_adapter_cb, *adapter_infos);
    if (0 != rv) {
        neu_persist_adapter_infos_free(*adapter_infos);
    }

    return rv;
}

int neu_persister_delete_adapter(neu_persister_t *persister,
                                 const char *     adapter_name)
{
    char path[PATH_MAX_SIZE] = { 0 };
    int  n = persister_adapter_dir(path, sizeof(path), persister, adapter_name);
    if (sizeof(path) == n) {
        nlog_error("persister path too long: %s", path);
        return -1;
    }
    rmdir_recursive(path);
    return 0;
}

int neu_persister_update_adapter(neu_persister_t *persister,
                                 const char *adapter_name, const char *new_name)
{
    char path[PATH_MAX_SIZE] = { 0 };
    int  n = persister_adapter_dir(path, sizeof(path), persister, adapter_name);
    if (sizeof(path) == n) {
        nlog_error("persister path too long: %s", path);
        return -1;
    }

    char new_path[PATH_MAX_SIZE] = { 0 };
    n = persister_adapter_dir(new_path, sizeof(new_path), persister, new_name);
    if (sizeof(new_path) == n) {
        nlog_error("persister new path too long: %s", new_path);
        return -1;
    }

    if (0 != rename(path, new_path)) {
        return -1;
    }

    return 0;
}

int neu_persister_store_plugins(neu_persister_t *persister,
                                UT_array *       plugin_infos)
{
    int                    index       = 0;
    neu_json_plugin_resp_t plugin_resp = {
        .n_plugin = utarray_len(plugin_infos),
    };

    plugin_resp.plugins = calloc(utarray_len(plugin_infos), sizeof(char *));

    utarray_foreach(plugin_infos, neu_resp_plugin_info_t *, plugin)
    {
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

    rv = write_file_string(persister->plugins_fname, result);

    free(result);
    return rv;
}

int neu_persister_load_plugins(neu_persister_t *persister,
                               UT_array **      plugin_infos)
{
    char *json_str = NULL;
    int   rv       = read_file_string(persister->plugins_fname, &json_str);
    if (rv != 0) {
        return rv;
    }

    neu_json_plugin_req_t *plugin_req = NULL;
    rv = neu_json_decode_plugin_req(json_str, &plugin_req);
    if (rv != 0) {
        free(json_str);
        return rv;
    }

    utarray_new(*plugin_infos, &ut_ptr_icd);
    for (int i = 0; i < plugin_req->n_plugin; i++) {
        char *name = plugin_req->plugins[i];
        utarray_push_back(*plugin_infos, &name);
    }

    free(json_str);
    free(plugin_req->plugins);
    free(plugin_req);
    return 0;
}

int neu_persister_store_datatags(neu_persister_t *persister,
                                 const char *     adapter_name,
                                 const char *     group_config_name,
                                 UT_array *       datatag_infos)
{
    char path[PATH_MAX_SIZE] = { 0 };

    int n = persister_group_config_dir(path, sizeof(path), persister,
                                       adapter_name, group_config_name);
    if (sizeof(path) == n) {
        nlog_error("path too long: %s", path);
        return -1;
    }

    int rv = create_dir_recursive(path);
    if (0 != rv) {
        nlog_error("fail to create dir: %s", path);
        return -1;
    }

    n = path_cat(path, n, sizeof(path), "datatags.json");
    if (sizeof(path) == n) {
        nlog_error("path too long: %s", path);
        return -1;
    }

    neu_json_datatag_req_t datatags_resp = { 0 };
    datatags_resp.n_tag                  = utarray_len(datatag_infos);
    datatags_resp.tags =
        calloc(datatags_resp.n_tag, sizeof(neu_json_datatag_req_tag_t));
    int index = 0;
    utarray_foreach(datatag_infos, neu_datatag_t *, tag)
    {
        datatags_resp.tags[index].name        = tag->name;
        datatags_resp.tags[index].description = tag->description;
        datatags_resp.tags[index].address     = tag->address;
        datatags_resp.tags[index].attribute   = tag->attribute;
        datatags_resp.tags[index].type        = tag->type;
        index += 1;
    }

    char *result = NULL;
    rv = neu_json_encode_by_fn(&datatags_resp, neu_json_encode_datatag_resp,
                               &result);

    if (rv == 0) {
        write_file_string(path, result);
    }

    free(result);
    free(datatags_resp.tags);

    return rv;
}

int neu_persister_load_datatags(neu_persister_t *persister,
                                const char *     adapter_name,
                                const char *group_config_name, UT_array **tags)
{
    char path[PATH_MAX_SIZE] = { 0 };

    int n = persister_group_config_dir(path, sizeof(path), persister,
                                       adapter_name, group_config_name);
    if (sizeof(path) == n) {
        nlog_error("path too long: %s", path);
        return -1;
    }

    n = path_cat(path, n, sizeof(path), "datatags.json");
    if (sizeof(path) == n) {
        nlog_error("path too long: %s", path);
        return -1;
    }

    char *json_str = NULL;
    int   rv       = read_file_string(path, &json_str);
    if (rv != 0) {
        return rv;
    }

    neu_json_datatag_req_t *datatag_req = NULL;
    rv = neu_json_decode_datatag_req(json_str, &datatag_req);
    free(json_str);
    if (rv != 0) {
        return rv;
    }

    utarray_new(*tags, neu_tag_get_icd());
    for (int i = 0; i < datatag_req->n_tag; i++) {
        neu_datatag_t tag = {
            .name        = datatag_req->tags[i].name,
            .address     = datatag_req->tags[i].address,
            .type        = datatag_req->tags[i].type,
            .attribute   = datatag_req->tags[i].attribute,
            .description = datatag_req->tags[i].description,
        };

        utarray_push_back(*tags, &tag);
    }

    neu_json_decode_datatag_req_free(datatag_req);

    return rv;
}

int neu_persister_store_subscriptions(neu_persister_t *persister,
                                      const char *     adapter_name,
                                      UT_array *       subscription_infos)
{
    char path[PATH_MAX_SIZE] = { 0 };

    int n = persister_adapter_dir(path, sizeof(path), persister, adapter_name);
    if (sizeof(path) == n) {
        nlog_error("path too long: %s", path);
        return -1;
    }

    int rv = create_dir(path);
    if (0 != rv) {
        nlog_error("fail to create dir: %s", path);
        return rv;
    }

    n = path_cat(path, n, sizeof(path), "subscriptions.json");
    if (sizeof(path) == n) {
        nlog_error("path too long: %s", path);
        return -1;
    }

    neu_json_subscriptions_req_t subs_resp = { 0 };
    int                          index     = 0;
    subs_resp.n_subscription               = utarray_len(subscription_infos);
    subs_resp.subscriptions =
        calloc(subs_resp.n_subscription,
               sizeof(neu_json_subscriptions_req_subscription_t));
    utarray_foreach(subscription_infos, neu_resp_subscribe_info_t *, info)
    {
        subs_resp.subscriptions[index].group_config_name = info->group;
        subs_resp.subscriptions[index].src_adapter_name  = info->driver;
        subs_resp.subscriptions[index].sub_adapter_name  = info->app;
        index += 1;
    }

    char *result = NULL;
    rv = neu_json_encode_by_fn(&subs_resp, neu_json_encode_subscriptions_resp,
                               &result);
    if (rv == 0) {
        rv = write_file_string(path, result);
    }

    free(result);
    free(subs_resp.subscriptions);

    return rv;
}

int neu_persister_load_subscriptions(neu_persister_t *persister,
                                     const char *     adapter_name,
                                     UT_array **      subscription_infos)
{
    char path[PATH_MAX_SIZE] = { 0 };

    int n = persister_adapter_dir(path, sizeof(path), persister, adapter_name);
    if (sizeof(path) == n) {
        nlog_error("path too long: %s", path);
        return -1;
    }

    n = path_cat(path, n, sizeof(path), "subscriptions.json");
    if (sizeof(path) == n) {
        nlog_error("path too long: %s", path);
        return -1;
    }

    char *json_str = NULL;
    int   rv       = read_file_string(path, &json_str);
    if (rv != 0) {
        return rv;
    }

    neu_json_subscriptions_req_t *subs_req = NULL;
    rv = neu_json_decode_subscriptions_req(json_str, &subs_req);
    free(json_str);
    if (0 != rv) {
        return rv;
    }

    UT_icd icd = { sizeof(neu_persist_subscription_info_t), NULL, NULL, NULL };
    utarray_new(*subscription_infos, &icd);

    for (int i = 0; i < subs_req->n_subscription; i++) {
        neu_persist_subscription_info_t info = { 0 };
        info.group_config_name =
            strdup(subs_req->subscriptions[i].group_config_name);
        info.src_adapter_name =
            strdup(subs_req->subscriptions[i].src_adapter_name);
        info.sub_adapter_name =
            strdup(subs_req->subscriptions[i].sub_adapter_name);

        utarray_push_back(*subscription_infos, &info);
    }

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
        nlog_error("persister path too long: %s", path);
        return -1;
    }

    int rv = create_dir_recursive(path);
    if (0 != rv) {
        nlog_error("persister failed to create dir: %s", path);
        return rv;
    }

    n = path_cat(path, n, sizeof(path), "config.json");
    if (sizeof(path) == n) {
        nlog_error("persister path too long: %s", path);
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
                                     UT_array **      group_config_infos)
{
    char   path[PATH_MAX_SIZE] = { 0 };
    UT_icd icd = { sizeof(neu_persist_group_config_info_t), NULL, NULL, NULL };

    int n = persister_group_configs_dir(path, sizeof(path), persister,
                                        adapter_name);
    if (sizeof(path) == n) {
        nlog_error("persister path too long: %s", path);
        return -1;
    }

    utarray_new(*group_config_infos, &icd);

    int rv = file_tree_walk(path, read_group_config_cb, *group_config_infos);
    if (0 != rv) {
        neu_persist_group_config_infos_free(*group_config_infos);
    }

    return rv;
}

int neu_persister_delete_group_config(neu_persister_t *persister,
                                      const char *     adapter_name,
                                      const char *     group_config_name)
{
    char path[PATH_MAX_SIZE] = { 0 };

    int n = persister_group_config_dir(path, sizeof(path), persister,
                                       adapter_name, group_config_name);
    if (sizeof(path) == n) {
        nlog_error("persister path too long: %s", path);
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
        nlog_error("persister path too long: %s", path);
        return -1;
    }
    if (0 != create_dir(path)) {
        nlog_error("persister failed to create dir: %s", path);
        return -1;
    }

    n = path_cat(path, n, sizeof(path), "settings.json");
    if (sizeof(path) == n) {
        nlog_error("persister path too long: %s", path);
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
        nlog_error("persister path too long: %s", path);
        return -1;
    }

    n = path_cat(path, n, sizeof(path), "settings.json");
    if (sizeof(path) == n) {
        nlog_error("persister path too long: %s", path);
        return -1;
    }

    int rv = read_file_string(path, (char **) setting);

    return rv;
}

int neu_persister_delete_adapter_setting(neu_persister_t *persister,
                                         const char *     adapter_name)
{
    (void) persister;
    (void) adapter_name;
    return 0;
}
