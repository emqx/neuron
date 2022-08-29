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
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <sqlite3.h>

#include "errcodes.h"
#include "utils/log.h"

#include "argparse.h"
#include "persist/json/persist_json_plugin.h"
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
    const char *plugins_fname;
    sqlite3 *   db;
} neu_persister_t;

const char *neu_persister_get_persist_dir(neu_persister_t *persister)
{
    if (persister == NULL) {
        return NULL;
    }

    return persister->persist_dir;
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

static inline int execute_sql(sqlite3 *db, const char *sql, ...)
{
    int rv = 0;

    va_list args;
    va_start(args, sql);
    char *query = sqlite3_vmprintf(sql, args);
    va_end(args);

    if (NULL == query) {
        nlog_error("allocate SQL `%s` fail", sql);
        return NEU_ERR_EINTERNAL;
    }

    char *err_msg = NULL;
    if (SQLITE_OK != sqlite3_exec(db, query, NULL, NULL, &err_msg)) {
        nlog_error("query `%s` fail: %s", query, err_msg);
        rv = NEU_ERR_EINTERNAL;
    } else {
        nlog_info("query %s success", query);
    }

    sqlite3_free(err_msg);
    sqlite3_free(query);

    return rv;
}

static int get_schema_version(sqlite3 *db, char **version_p, bool *dirty_p)
{
    sqlite3_stmt *stmt  = NULL;
    const char *  query = "SELECT version, dirty FROM migrations ORDER BY "
                        "migration_id DESC LIMIT 1";

    if (SQLITE_OK != sqlite3_prepare_v2(db, query, -1, &stmt, NULL)) {
        nlog_error("prepare `%s` fail: %s", query, sqlite3_errmsg(db));
        return NEU_ERR_EINTERNAL;
    }

    int step = sqlite3_step(stmt);
    if (SQLITE_ROW == step) {
        char *version = strdup((char *) sqlite3_column_text(stmt, 0));
        if (NULL == version) {
            sqlite3_finalize(stmt);
            return NEU_ERR_EINTERNAL;
        }

        *version_p = version;
        *dirty_p   = sqlite3_column_int(stmt, 1);
    } else if (SQLITE_DONE != step) {
        nlog_warn("query `%s` fail: %s", query, sqlite3_errmsg(db));
    }

    sqlite3_finalize(stmt);
    return 0;
}

int schema_version_cmp(const void *a, const void *b)
{
    return strcmp(*(char **) a, *(char **) b);
}

static int extract_schema_info(const char *file, char **version_p,
                               char **description_p)
{
    if (!ends_with(file, ".sql")) {
        return NEU_ERR_EINTERNAL;
    }

    char *sep = strchr(file, '_');
    if (NULL == sep) {
        return NEU_ERR_EINTERNAL;
    }

    size_t n = sep - file;
    if (4 != n) {
        // invalid version
        return NEU_ERR_EINTERNAL;
    }

    if (strcmp(++sep, ".sql") == 0) {
        // no description
        return NEU_ERR_EINTERNAL;
    }

    char *version = calloc(n + 1, sizeof(char));
    if (NULL == version) {
        return NEU_ERR_EINTERNAL;
    }
    strncat(version, file, n);

    n                 = strlen(sep) - 4;
    char *description = calloc(n + 1, sizeof(char));
    if (NULL == description) {
        free(version);
        return NEU_ERR_EINTERNAL;
    }
    strncat(description, sep, n);

    *version_p     = version;
    *description_p = description;

    return 0;
}

static int apply_schema_file(sqlite3 *db, const char *dir, const char *file)
{
    int   rv          = 0;
    char *version     = NULL;
    char *description = NULL;
    char *sql         = NULL;
    char  path[128]   = {};
    int   n           = 0;

    if (sizeof(path) == (n = path_cat(path, 0, sizeof(path), dir)) ||
        sizeof(path) == path_cat(path, n, sizeof(path), file)) {
        nlog_error("path too long: %s", path);
        return -1;
    }

    if (0 != extract_schema_info(file, &version, &description)) {
        nlog_warn("extract `%s` schema info fail, ignore", path);
        return 0;
    }

    rv = read_file_string(path, &sql);
    if (0 != rv) {
        goto end;
    }

    rv = execute_sql(db,
                     "INSERT INTO migrations (version, description, dirty) "
                     "VALUES (%Q, %Q, 1)",
                     version, description);
    if (0 != rv) {
        goto end;
    }

    char *err_msg = NULL;
    rv            = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if (SQLITE_OK != rv) {
        nlog_error("execute %s fail: %s", path, err_msg);
        sqlite3_free(err_msg);
        rv = NEU_ERR_EINTERNAL;
        goto end;
    }

    rv = execute_sql(db, "UPDATE migrations SET dirty = 0 WHERE version=%Q",
                     version);

end:
    if (0 == rv) {
        nlog_info("success apply schema `%s`, version=`%s` description=`%s`",
                  path, version, description);
    } else {
        nlog_error("fail apply schema `%s`, version=`%s` description=`%s`",
                   path, version, description);
    }

    free(sql);
    free(version);
    free(description);
    return rv;
}

static int apply_schemas(sqlite3 *db, const char *dir)
{
    int rv = 0;

    const char *sql = "CREATE TABLE IF NOT EXISTS migrations ( migration_id "
                      "INTEGER PRIMARY KEY, version TEXT NOT NULL UNIQUE, "
                      "description TEXT NOT NULL, dirty INTEGER NOT NULL, "
                      "created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP) ";
    if (0 != execute_sql(db, sql)) {
        nlog_error("create migration table fail");
        return NEU_ERR_EINTERNAL;
    }

    bool  dirty   = false;
    char *version = NULL;
    if (0 != get_schema_version(db, &version, &dirty)) {
        nlog_error("find schema version fail");
        return NEU_ERR_EINTERNAL;
    }

    nlog_info("schema head version=%s", version ? version : "none");

    if (dirty) {
        nlog_error("database is dirty, need manual intervention");
        return NEU_ERR_EINTERNAL;
    }

    DIR *          dirp = NULL;
    struct dirent *dent = NULL;
    if ((dirp = opendir(dir)) == NULL) {
        nlog_error("fail open dir: %s", dir);
        free(version);
        return NEU_ERR_EINTERNAL;
    }

    UT_array *files = NULL;
    utarray_new(files, &ut_str_icd);

    while (NULL != (dent = readdir(dirp))) {
        if (ends_with(dent->d_name, ".sql")) {
            char *file = dent->d_name;
            utarray_push_back(files, &file);
        }
    }

    closedir(dirp);
    dirp = NULL;

    if (0 == utarray_len(files)) {
        nlog_warn("directory `%s` contains no schema files", dir);
    }

    utarray_sort(files, schema_version_cmp);

    utarray_foreach(files, char **, file)
    {
        if (version && strncmp(*file, version, strlen(version)) <= 0) {
            continue;
        }

        if (0 != apply_schema_file(db, dir, *file)) {
            rv = NEU_ERR_EINTERNAL;
            break;
        }
    }

    free(version);
    utarray_free(files);

    return rv;
}

neu_persister_t *neu_persister_create(const char *dir_name)
{
    int   rv                  = 0;
    char *persist_dir         = NULL;
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

    int n = path_cat(path, dir_len, sizeof(path), "plugins.json");
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

    persister->plugins_fname = plugins_fname;
    persister->persist_dir   = persist_dir;

    n = path_cat(path, dir_len, sizeof(path), "sqlite.db");
    if (sizeof(path) == n) {
        nlog_error("path too long: %s", path);
        goto error;
    }
    rv = sqlite3_open(path, &persister->db);
    if (SQLITE_OK != rv) {
        nlog_error("db `%s` fail: %s", path, sqlite3_errstr(rv));
        goto error;
    }
    sqlite3_busy_timeout(persister->db, 100 * 1000);
    // we rely on sqlite foreign key support
    if (SQLITE_OK !=
        sqlite3_exec(persister->db, "PRAGMA foreign_keys=ON", NULL, NULL,
                     NULL)) {
        nlog_error("db foreign key support fail: %s",
                   sqlite3_errmsg(persister->db));
        sqlite3_close(persister->db);
        goto error;
    }
    if (0 != apply_schemas(persister->db, g_config_dir)) {
        goto error;
    }

    return persister;

error:
    free(plugins_fname);
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
    sqlite3_close(persister->db);
    free((char *) persister->plugins_fname);
    free((char *) persister->persist_dir);
    free(persister);
}

int neu_persister_store_node(neu_persister_t *        persister,
                             neu_persist_node_info_t *info)
{
    return execute_sql(persister->db,
                       "INSERT INTO nodes (name, type, state, plugin_name) "
                       "VALUES (%Q, %i, %i, %Q)",
                       info->name, info->type, info->state, info->plugin_name);
}

static UT_icd node_info_icd = {
    sizeof(neu_persist_node_info_t),
    NULL,
    NULL,
    (dtor_f *) neu_persist_node_info_fini,
};

int neu_persister_load_nodes(neu_persister_t *persister, UT_array **node_infos)
{
    int           rv    = 0;
    sqlite3_stmt *stmt  = NULL;
    const char *  query = "SELECT name, type, state, plugin_name FROM nodes;";

    utarray_new(*node_infos, &node_info_icd);

    if (SQLITE_OK !=
        sqlite3_prepare_v2(persister->db, query, -1, &stmt, NULL)) {
        nlog_error("prepare `%s` fail: %s", query,
                   sqlite3_errmsg(persister->db));
        utarray_free(*node_infos);
        *node_infos = NULL;
        return NEU_ERR_EINTERNAL;
    }

    int step = sqlite3_step(stmt);
    while (SQLITE_ROW == step) {
        neu_persist_node_info_t info = {};
        char *name = strdup((char *) sqlite3_column_text(stmt, 0));
        if (NULL == name) {
            break;
        }

        char *plugin_name = strdup((char *) sqlite3_column_text(stmt, 3));
        if (NULL == plugin_name) {
            free(name);
            break;
        }

        info.name        = name;
        info.type        = sqlite3_column_int(stmt, 1);
        info.state       = sqlite3_column_int(stmt, 2);
        info.plugin_name = plugin_name;
        utarray_push_back(*node_infos, &info);

        step = sqlite3_step(stmt);
    }

    if (SQLITE_DONE != step) {
        nlog_warn("query `%s` fail: %s", query, sqlite3_errmsg(persister->db));
        // do not set return code, return partial or empty result
    }

    sqlite3_finalize(stmt);
    return rv;
}

int neu_persister_delete_node(neu_persister_t *persister, const char *node_name)
{
    // rely on foreign key constraints to remove settings, groups, tags and
    // subscriptions
    return execute_sql(persister->db, "DELETE FROM nodes WHERE name=%Q;",
                       node_name);
}

int neu_persister_update_node(neu_persister_t *persister, const char *node_name,
                              const char *new_name)
{
    return execute_sql(persister->db, "UPDATE nodes SET name=%Q WHERE name=%Q;",
                       new_name, node_name);
}

int neu_persister_update_node_state(neu_persister_t *persister,
                                    const char *node_name, int state)
{
    return execute_sql(persister->db,
                       "UPDATE nodes SET state=%i WHERE name=%Q;", state,
                       node_name);
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
        if (NEU_PLUGIN_KIND_SYSTEM == plugin->kind) {
            // filter out system plugins
            continue;
        }
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

int neu_persister_load_plugins(neu_persister_t *persister,
                               UT_array **      plugin_infos)
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
    if (0 != load_plugins_file(persister->plugins_fname, user_plugins)) {
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

int neu_persister_store_tag(neu_persister_t *persister, const char *driver_name,
                            const char *group_name, const neu_datatag_t *tag)
{
    return execute_sql(persister->db,
                       "INSERT INTO tags (driver_name, group_name, name, "
                       "address, attribute, precision, "
                       "type, decimal, description) VALUES(%Q, %Q, %Q, %Q, %i, "
                       "%i, %i, %lf, %Q)",
                       driver_name, group_name, tag->name, tag->address,
                       tag->attribute, tag->precision, tag->type, tag->decimal,
                       tag->description);
}

int neu_persister_load_tags(neu_persister_t *persister, const char *driver_name,
                            const char *group_name, UT_array **tags)
{
    sqlite3_stmt *stmt  = NULL;
    const char *  query = "SELECT name, address, attribute, precision, type, "
                        "decimal, description "
                        "FROM tags WHERE driver_name=? AND group_name=?";

    utarray_new(*tags, neu_tag_get_icd());

    if (SQLITE_OK !=
        sqlite3_prepare_v2(persister->db, query, -1, &stmt, NULL)) {
        nlog_error("prepare `%s` fail: %s", query,
                   sqlite3_errmsg(persister->db));
        goto error;
    }

    if (SQLITE_OK != sqlite3_bind_text(stmt, 1, driver_name, -1, NULL)) {
        nlog_error("bind `%s` with `%s` fail: %s", query, driver_name,
                   sqlite3_errmsg(persister->db));
        goto error;
    }

    if (SQLITE_OK != sqlite3_bind_text(stmt, 2, group_name, -1, NULL)) {
        nlog_error("bind `%s` with `%s` fail: %s", query, group_name,
                   sqlite3_errmsg(persister->db));
        goto error;
    }

    int step = sqlite3_step(stmt);
    while (SQLITE_ROW == step) {
        neu_datatag_t tag = {
            .name        = (char *) sqlite3_column_text(stmt, 0),
            .address     = (char *) sqlite3_column_text(stmt, 1),
            .attribute   = sqlite3_column_int(stmt, 2),
            .precision   = sqlite3_column_int64(stmt, 3),
            .type        = sqlite3_column_int(stmt, 4),
            .decimal     = sqlite3_column_double(stmt, 5),
            .description = (char *) sqlite3_column_text(stmt, 6),
        };
        utarray_push_back(*tags, &tag);

        step = sqlite3_step(stmt);
    }

    if (SQLITE_DONE != step) {
        nlog_warn("query `%s` fail: %s", query, sqlite3_errmsg(persister->db));
        // do not set return code, return partial or empty result
    }

    sqlite3_finalize(stmt);
    return 0;

error:
    utarray_free(*tags);
    *tags = NULL;
    return NEU_ERR_EINTERNAL;
}

int neu_persister_update_tag(neu_persister_t *persister,
                             const char *driver_name, const char *group_name,
                             const neu_datatag_t *tag)
{
    return execute_sql(persister->db,
                       "UPDATE tags SET address=%Q, attribute=%i, "
                       "precision=%i, type=%i, decimal=%lf, description=%Q "
                       "WHERE driver_name=%Q AND group_name=%Q AND name=%Q",
                       tag->address, tag->attribute, tag->precision, tag->type,
                       tag->decimal, tag->description, driver_name, group_name,
                       tag->name);
}

int neu_persister_delete_tag(neu_persister_t *persister,
                             const char *driver_name, const char *group_name,
                             const char *tag_name)
{
    return execute_sql(
        persister->db,
        "DELETE FROM tags WHERE driver_name=%Q AND group_name=%Q AND name=%Q",
        driver_name, group_name, tag_name);
}

int neu_persister_store_subscription(neu_persister_t *persister,
                                     const char *     app_name,
                                     const char *     driver_name,
                                     const char *     group_name)
{
    return execute_sql(
        persister->db,
        "INSERT INTO subscriptions (app_name, driver_name, group_name) "
        "VALUES (%Q, %Q, %Q)",
        app_name, driver_name, group_name);
}

static UT_icd subscription_info_icd = {
    sizeof(neu_persist_subscription_info_t),
    NULL,
    NULL,
    (dtor_f *) neu_persist_subscription_info_fini,
};

int neu_persister_load_subscriptions(neu_persister_t *persister,
                                     const char *     app_name,
                                     UT_array **      subscription_infos)
{
    sqlite3_stmt *stmt = NULL;
    const char *  query =
        "SELECT driver_name, group_name FROM subscriptions WHERE app_name=?";

    utarray_new(*subscription_infos, &subscription_info_icd);

    if (SQLITE_OK !=
        sqlite3_prepare_v2(persister->db, query, -1, &stmt, NULL)) {
        nlog_error("prepare `%s` fail: %s", query,
                   sqlite3_errmsg(persister->db));
        goto error;
    }

    if (SQLITE_OK != sqlite3_bind_text(stmt, 1, app_name, -1, NULL)) {
        nlog_error("bind `%s` with `%s` fail: %s", query, app_name,
                   sqlite3_errmsg(persister->db));
        goto error;
    }

    int step = sqlite3_step(stmt);
    while (SQLITE_ROW == step) {
        char *driver_name = strdup((char *) sqlite3_column_text(stmt, 0));
        if (NULL == driver_name) {
            break;
        }

        char *group_name = strdup((char *) sqlite3_column_text(stmt, 1));
        if (NULL == group_name) {
            free(driver_name);
            break;
        }

        neu_persist_subscription_info_t info = {
            .driver_name = driver_name,
            .group_name  = group_name,
        };
        utarray_push_back(*subscription_infos, &info);

        step = sqlite3_step(stmt);
    }

    if (SQLITE_DONE != step) {
        nlog_warn("query `%s` fail: %s", query, sqlite3_errmsg(persister->db));
        // do not set return code, return partial or empty result
    }

    sqlite3_finalize(stmt);
    return 0;

error:
    utarray_free(*subscription_infos);
    *subscription_infos = NULL;
    return NEU_ERR_EINTERNAL;
}

int neu_persister_delete_subscription(neu_persister_t *persister,
                                      const char *     app_name,
                                      const char *     driver_name,
                                      const char *     group_name)
{
    return execute_sql(persister->db,
                       "DELETE FROM subscriptions WHERE app_name=%Q AND "
                       "driver_name=%Q AND group_name=%Q",
                       app_name, driver_name, group_name);
}

int neu_persister_store_group(neu_persister_t *         persister,
                              const char *              driver_name,
                              neu_persist_group_info_t *group_info)
{
    return execute_sql(
        persister->db,
        "INSERT INTO groups (driver_name, name, interval) VALUES (%Q, %Q, %u)",
        driver_name, group_info->name, (unsigned) group_info->interval);
}

int neu_persister_update_group(neu_persister_t *         persister,
                               const char *              driver_name,
                               neu_persist_group_info_t *group_info)
{
    return execute_sql(persister->db,
                       "UPDATE groups SET driver_name=%Q, name=%Q, "
                       "interval=%i, WHERE driver_name=%Q AND name=%Q",
                       driver_name, group_info->name, group_info->interval,
                       driver_name, group_info->name);
}

static UT_icd group_info_icd = {
    sizeof(neu_persist_group_info_t),
    NULL,
    NULL,
    (dtor_f *) neu_persist_group_info_fini,
};

int neu_persister_load_groups(neu_persister_t *persister,
                              const char *driver_name, UT_array **group_infos)
{
    sqlite3_stmt *stmt = NULL;
    const char *query = "SELECT name, interval FROM groups WHERE driver_name=?";

    utarray_new(*group_infos, &group_info_icd);

    if (SQLITE_OK !=
        sqlite3_prepare_v2(persister->db, query, -1, &stmt, NULL)) {
        nlog_error("prepare `%s` fail: %s", query,
                   sqlite3_errmsg(persister->db));
        goto error;
    }

    if (SQLITE_OK != sqlite3_bind_text(stmt, 1, driver_name, -1, NULL)) {
        nlog_error("bind `%s` with `%s` fail: %s", query, driver_name,
                   sqlite3_errmsg(persister->db));
        goto error;
    }

    int step = sqlite3_step(stmt);
    while (SQLITE_ROW == step) {
        neu_persist_group_info_t info = {};
        char *name = strdup((char *) sqlite3_column_text(stmt, 0));
        if (NULL == name) {
            break;
        }

        info.name     = name;
        info.interval = sqlite3_column_int(stmt, 1);
        utarray_push_back(*group_infos, &info);

        step = sqlite3_step(stmt);
    }

    if (SQLITE_DONE != step) {
        nlog_warn("query `%s` fail: %s", query, sqlite3_errmsg(persister->db));
        // do not set return code, return partial or empty result
    }

    sqlite3_finalize(stmt);
    return 0;

error:
    utarray_free(*group_infos);
    *group_infos = NULL;
    return NEU_ERR_EINTERNAL;
}

int neu_persister_delete_group(neu_persister_t *persister,
                               const char *driver_name, const char *group_name)
{
    // rely on foreign key constraints to delete tags and subscriptions
    return execute_sql(persister->db,
                       "DELETE FROM groups WHERE driver_name=%Q AND name=%Q",
                       driver_name, group_name);
}

int neu_persister_store_node_setting(neu_persister_t *persister,
                                     const char *node_name, const char *setting)
{
    return execute_sql(
        persister->db,
        "INSERT OR REPLACE INTO settings (node_name, setting) VALUES (%Q, %Q)",
        node_name, setting);
}

int neu_persister_load_node_setting(neu_persister_t *  persister,
                                    const char *       node_name,
                                    const char **const setting)
{
    int           rv    = 0;
    sqlite3_stmt *stmt  = NULL;
    const char *  query = "SELECT setting FROM settings WHERE node_name=?";

    if (SQLITE_OK !=
        sqlite3_prepare_v2(persister->db, query, -1, &stmt, NULL)) {
        nlog_error("prepare `%s` with `%s` fail: %s", query, node_name,
                   sqlite3_errmsg(persister->db));
        return NEU_ERR_EINTERNAL;
    }

    if (SQLITE_OK != sqlite3_bind_text(stmt, 1, node_name, -1, NULL)) {
        nlog_error("bind `%s` with `%s` fail: %s", query, node_name,
                   sqlite3_errmsg(persister->db));
        rv = NEU_ERR_EINTERNAL;
        goto end;
    }

    if (SQLITE_ROW != sqlite3_step(stmt)) {
        nlog_warn("SQL `%s` with `%s` fail: %s", query, node_name,
                  sqlite3_errmsg(persister->db));
        rv = NEU_ERR_EINTERNAL;
        goto end;
    }

    char *s = strdup((char *) sqlite3_column_text(stmt, 0));
    if (NULL == s) {
        nlog_error("strdup fail");
        rv = NEU_ERR_EINTERNAL;
        goto end;
    }

    *setting = s;

end:
    sqlite3_finalize(stmt);
    return rv;
}

int neu_persister_delete_node_setting(neu_persister_t *persister,
                                      const char *     node_name)
{
    return execute_sql(persister->db, "DELETE FROM settings WHERE node_name=%Q",
                       node_name);
}
