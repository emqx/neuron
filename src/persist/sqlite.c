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
#include <sys/types.h>

#include <sqlite3.h>

#include "errcodes.h"
#include "utils/log.h"

#include "sqlite.h"

#if defined _WIN32 || defined __CYGWIN__
#define PATH_SEP_CHAR '\\'
#else
#define PATH_SEP_CHAR '/'
#endif

#define PATH_MAX_SIZE 128

#define DB_FILE "persistence/sqlite.db"

static inline bool ends_with(const char *str, const char *suffix)
{
    size_t m = strlen(str);
    size_t n = strlen(suffix);
    return m >= n && !strcmp(str + m - n, suffix);
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
                        "version DESC LIMIT 1";

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

static int schema_version_cmp(const void *a, const void *b)
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

static int should_apply(sqlite3 *db, const char *version)
{
    int           rv   = 0;
    sqlite3_stmt *stmt = NULL;
    const char *  sql  = "SELECT count(*) FROM migrations WHERE version=?";

    if (SQLITE_OK != sqlite3_prepare_v2(db, sql, -1, &stmt, NULL)) {
        nlog_error("prepare `%s` fail: %s", sql, sqlite3_errmsg(db));
        rv = -1;
        goto end;
    }

    if (SQLITE_OK != sqlite3_bind_text(stmt, 1, version, -1, NULL)) {
        nlog_error("bind `%s` with version=`%s` fail: %s", sql, version,
                   sqlite3_errmsg(db));
        rv = -1;
        goto end;
    }

    if (SQLITE_ROW == sqlite3_step(stmt)) {
        rv = sqlite3_column_int(stmt, 0) == 0 ? 1 : 0;
    } else {
        nlog_warn("query `%s` fail: %s", sql, sqlite3_errmsg(db));
        rv = -1;
    }

end:
    sqlite3_finalize(stmt);
    return rv;
}

static int apply_schema_file(sqlite3 *db, const char *dir, const char *file)
{
    int   rv          = 0;
    char *version     = NULL;
    char *description = NULL;
    char *sql         = NULL;
    char *path        = NULL;
    int   n           = 0;

    path = calloc(PATH_MAX_SIZE, sizeof(char));
    if (NULL == path) {
        nlog_error("malloc fail");
        return -1;
    }

    if (PATH_MAX_SIZE <= (n = path_cat(path, 0, PATH_MAX_SIZE, dir)) ||
        PATH_MAX_SIZE <= path_cat(path, n, PATH_MAX_SIZE, file)) {
        nlog_error("path too long: %s", path);
        free(path);
        return -1;
    }

    if (0 != extract_schema_info(file, &version, &description)) {
        nlog_warn("extract `%s` schema info fail, ignore", path);
        free(path);
        return 0;
    }

    rv = should_apply(db, version);
    if (rv <= 0) {
        free(version);
        free(description);
        free(path);
        return rv;
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
        nlog_error("execute %s fail: (%d)%s", path, rv, err_msg);
        sqlite3_free(err_msg);
        rv = NEU_ERR_EINTERNAL;
        goto end;
    }

    rv = execute_sql(db, "UPDATE migrations SET dirty = 0 WHERE version=%Q",
                     version);

end:
    if (0 == rv) {
        nlog_notice("success apply schema `%s`, version=`%s` description=`%s`",
                    path, version, description);
    } else {
        nlog_error("fail apply schema `%s`, version=`%s` description=`%s`",
                   path, version, description);
    }

    free(sql);
    free(path);
    free(version);
    free(description);
    return rv;
}

static UT_array *collect_schemas(const char *dir)
{
    DIR *          dirp  = NULL;
    struct dirent *dent  = NULL;
    UT_array *     files = NULL;

    if ((dirp = opendir(dir)) == NULL) {
        nlog_error("fail open dir: %s", dir);
        return NULL;
    }

    utarray_new(files, &ut_str_icd);

    while (NULL != (dent = readdir(dirp))) {
        if (ends_with(dent->d_name, ".sql")) {
            char *file = dent->d_name;
            utarray_push_back(files, &file);
        }
    }

    closedir(dirp);
    return files;
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

    nlog_notice("schema head version=%s", version ? version : "none");

    if (dirty) {
        nlog_error("database is dirty, need manual intervention");
        return NEU_ERR_EINTERNAL;
    }

    UT_array *files = collect_schemas(dir);
    if (NULL == files) {
        free(version);
        return NEU_ERR_EINTERNAL;
    }

    if (0 == utarray_len(files)) {
        nlog_warn("directory `%s` contains no schema files", dir);
    }

    utarray_sort(files, schema_version_cmp);

    utarray_foreach(files, char **, file)
    {
        if (0 != apply_schema_file(db, dir, *file)) {
            rv = NEU_ERR_EINTERNAL;
            break;
        }
    }

    free(version);
    utarray_free(files);

    return rv;
}

static inline int open_db(const char *schema_dir, sqlite3 **db_p)
{
    sqlite3 *db = NULL;
    int      rv = sqlite3_open(DB_FILE, &db);
    if (SQLITE_OK != rv) {
        nlog_fatal("db `%s` fail: %s", DB_FILE, sqlite3_errstr(rv));
        return -1;
    }
    sqlite3_busy_timeout(db, 100 * 1000);

    rv = sqlite3_exec(db, "PRAGMA foreign_keys=ON", NULL, NULL, NULL);
    if (rv != SQLITE_OK) {
        nlog_fatal("db foreign key support fail: %s", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }

    rv = sqlite3_exec(db, "PRAGMA journal_mode=WAL", NULL, NULL, NULL);
    if (rv != SQLITE_OK) {
        nlog_fatal("db journal_mode WAL fail: %s", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }

    rv = apply_schemas(db, schema_dir);
    if (rv != 0) {
        nlog_fatal("db apply schemas fail");
        sqlite3_close(db);
        return -1;
    }

    *db_p = db;
    return 0;
}

static struct neu_persister_vtbl_s g_sqlite_persister_vtbl = {
    .destroy                  = neu_sqlite_persister_destroy,
    .native_handle            = neu_sqlite_persister_native_handle,
    .store_node               = neu_sqlite_persister_store_node,
    .load_nodes               = neu_sqlite_persister_load_nodes,
    .delete_node              = neu_sqlite_persister_delete_node,
    .update_node              = neu_sqlite_persister_update_node,
    .update_node_state        = neu_sqlite_persister_update_node_state,
    .store_tag                = neu_sqlite_persister_store_tag,
    .store_tags               = neu_sqlite_persister_store_tags,
    .load_tags                = neu_sqlite_persister_load_tags,
    .update_tag               = neu_sqlite_persister_update_tag,
    .update_tag_value         = neu_sqlite_persister_update_tag_value,
    .delete_tag               = neu_sqlite_persister_delete_tag,
    .store_subscription       = neu_sqlite_persister_store_subscription,
    .update_subscription      = neu_sqlite_persister_update_subscription,
    .load_subscriptions       = neu_sqlite_persister_load_subscriptions,
    .delete_subscription      = neu_sqlite_persister_delete_subscription,
    .store_group              = neu_sqlite_persister_store_group,
    .update_group             = neu_sqlite_persister_update_group,
    .load_groups              = neu_sqlite_persister_load_groups,
    .delete_group             = neu_sqlite_persister_delete_group,
    .store_node_setting       = neu_sqlite_persister_store_node_setting,
    .load_node_setting        = neu_sqlite_persister_load_node_setting,
    .delete_node_setting      = neu_sqlite_persister_delete_node_setting,
    .load_users               = neu_sqlite_persister_load_users,
    .store_user               = neu_sqlite_persister_store_user,
    .update_user              = neu_sqlite_persister_update_user,
    .load_user                = neu_sqlite_persister_load_user,
    .delete_user              = neu_sqlite_persister_delete_user,
    .store_server_cert        = neu_sqlite_persister_store_server_cert,
    .update_server_cert       = neu_sqlite_persister_update_server_cert,
    .load_server_cert         = neu_sqlite_persister_load_server_cert,
    .store_client_cert        = neu_sqlite_persister_store_client_cert,
    .update_client_cert       = neu_sqlite_persister_update_client_cert,
    .load_client_certs_by_app = neu_sqlite_persister_load_client_certs_by_app,
    .load_client_certs        = neu_sqlite_persister_load_client_certs,
    .delete_client_cert       = neu_sqlite_persister_delete_client_cert,
    .store_security_policy    = neu_sqlite_persister_store_security_policy,
    .update_security_policy   = neu_sqlite_persister_update_security_policy,
    .load_security_policy     = neu_sqlite_persister_load_security_policy,
    .load_security_policies   = neu_sqlite_persister_load_security_policies,
    .store_auth_setting       = neu_sqlite_persister_store_auth_setting,
    .update_auth_setting      = neu_sqlite_persister_update_auth_setting,
    .load_auth_setting        = neu_sqlite_persister_load_auth_setting,
    .store_auth_user          = neu_sqlite_persister_store_auth_user,
    .update_auth_user         = neu_sqlite_persister_update_auth_user,
    .load_auth_user           = neu_sqlite_persister_load_auth_user,
    .load_auth_users_by_app   = neu_sqlite_persister_load_auth_users_by_app,
    .delete_auth_user         = neu_sqlite_persister_delete_auth_user,
    .update_node_tags         = neu_sqlite_persister_update_node_tags,
};

neu_persister_t *neu_sqlite_persister_create(const char *schema_dir)
{
    neu_sqlite_persister_t *persister = calloc(1, sizeof(*persister));
    if (NULL == persister) {
        return NULL;
    }

    persister->vtbl = &g_sqlite_persister_vtbl;

    if (0 != open_db(schema_dir, &persister->db)) {
        free(persister);
        return NULL;
    }

    return (neu_persister_t *) persister;
}

void *neu_sqlite_persister_native_handle(neu_persister_t *self)
{
    return ((neu_sqlite_persister_t *) self)->db;
}

void neu_sqlite_persister_destroy(neu_persister_t *self)
{
    neu_sqlite_persister_t *persister = (neu_sqlite_persister_t *) self;
    if (persister) {
        sqlite3_close(persister->db);
        free(persister);
    }
}

int neu_sqlite_persister_store_node(neu_persister_t *        self,
                                    neu_persist_node_info_t *info)
{
    int rv = 0;
    rv     = execute_sql(((neu_sqlite_persister_t *) self)->db,
                     "INSERT INTO nodes (name, type, state, plugin_name, tags) "
                     "VALUES (%Q, %i, %i, %Q, %Q)",
                     info->name, info->type, info->state, info->plugin_name,
                     info->tags);
    return rv;
}

static UT_icd node_info_icd = {
    sizeof(neu_persist_node_info_t),
    NULL,
    NULL,
    (dtor_f *) neu_persist_node_info_fini,
};

int neu_sqlite_persister_load_nodes(neu_persister_t *self,
                                    UT_array **      node_infos)
{
    neu_sqlite_persister_t *persister = (neu_sqlite_persister_t *) self;

    int           rv   = 0;
    sqlite3_stmt *stmt = NULL;
    const char *  query =
        "SELECT name, type, state, plugin_name, tags FROM nodes;";

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

        char *tags = (char *) sqlite3_column_text(stmt, 4);

        info.name        = name;
        info.type        = sqlite3_column_int(stmt, 1);
        info.state       = sqlite3_column_int(stmt, 2);
        info.plugin_name = plugin_name;
        if (tags) {
            info.tags = strdup(tags);
        } else {
            info.tags = NULL;
        }

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

int neu_sqlite_persister_delete_node(neu_persister_t *self,
                                     const char *     node_name)
{
    // rely on foreign key constraints to remove settings, groups, tags and
    // subscriptions
    int rv = execute_sql(((neu_sqlite_persister_t *) self)->db,
                         "DELETE FROM nodes WHERE name=%Q;", node_name);
    return rv;
}

int neu_sqlite_persister_update_node(neu_persister_t *self,
                                     const char *     node_name,
                                     const char *     new_name)
{
    return execute_sql(((neu_sqlite_persister_t *) self)->db,
                       "UPDATE nodes SET name=%Q WHERE name=%Q;", new_name,
                       node_name);
}

int neu_sqlite_persister_update_node_tags(neu_persister_t *self,
                                          const char *     node_name,
                                          const char *     tags)
{
    return execute_sql(((neu_sqlite_persister_t *) self)->db,
                       "UPDATE nodes SET tags=%Q WHERE name=%Q;", tags,
                       node_name);
}

int neu_sqlite_persister_update_node_state(neu_persister_t *self,
                                           const char *node_name, int state)
{
    return execute_sql(((neu_sqlite_persister_t *) self)->db,
                       "UPDATE nodes SET state=%i WHERE name=%Q;", state,
                       node_name);
}

int neu_sqlite_persister_store_tag(neu_persister_t *    self,
                                   const char *         driver_name,
                                   const char *         group_name,
                                   const neu_datatag_t *tag)
{
    char format_buf[128] = { 0 };
    if (tag->n_format > 0) {
        neu_tag_format_str(tag, format_buf, sizeof(format_buf));
    }

    int rv = execute_sql(
        ((neu_sqlite_persister_t *) self)->db,
        "INSERT INTO tags ("
        " driver_name, group_name, name, address, attribute,"
        " precision, type, decimal, bias, description, value, format"
        ") VALUES (%Q, %Q, %Q, %Q, %i, %i, %i, %lf, %lf, %Q, %Q, %Q)",
        driver_name, group_name, tag->name, tag->address, tag->attribute,
        tag->precision, tag->type, tag->decimal, tag->bias, tag->description,
        "", format_buf);

    return rv;
}

static int put_tags(sqlite3 *db, const char *query, sqlite3_stmt *stmt,
                    const neu_datatag_t *tags, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        const neu_datatag_t *tag             = &tags[i];
        char                 format_buf[128] = { 0 };

        if (tag->n_format > 0) {
            neu_tag_format_str(tag, format_buf, sizeof(format_buf));
        }

        sqlite3_reset(stmt);

        if (SQLITE_OK != sqlite3_bind_text(stmt, 3, tag->name, -1, NULL)) {
            nlog_error("bind `%s` with name=`%s` fail: %s", query, tag->name,
                       sqlite3_errmsg(db));
            return -1;
        }

        if (SQLITE_OK != sqlite3_bind_text(stmt, 4, tag->address, -1, NULL)) {
            nlog_error("bind `%s` with address=`%s` fail: %s", query,
                       tag->address, sqlite3_errmsg(db));
            return -1;
        }

        if (SQLITE_OK != sqlite3_bind_int(stmt, 5, tag->attribute)) {
            nlog_error("bind `%s` with attribute=`%i` fail: %s", query,
                       tag->attribute, sqlite3_errmsg(db));
            return -1;
        }

        if (SQLITE_OK != sqlite3_bind_int(stmt, 6, tag->precision)) {
            nlog_error("bind `%s` with precision=`%i` fail: %s", query,
                       tag->precision, sqlite3_errmsg(db));
            return -1;
        }

        if (SQLITE_OK != sqlite3_bind_int(stmt, 7, tag->type)) {
            nlog_error("bind `%s` with type=`%i` fail: %s", query, tag->type,
                       sqlite3_errmsg(db));
            return -1;
        }

        if (SQLITE_OK != sqlite3_bind_double(stmt, 8, tag->decimal)) {
            nlog_error("bind `%s` with decimal=`%f` fail: %s", query,
                       tag->decimal, sqlite3_errmsg(db));
            return -1;
        }

        if (SQLITE_OK != sqlite3_bind_double(stmt, 9, tag->bias)) {
            nlog_error("bind `%s` with bias=`%f` fail: %s", query, tag->bias,
                       sqlite3_errmsg(db));
            return -1;
        }

        if (SQLITE_OK !=
            sqlite3_bind_text(stmt, 10, tag->description, -1, NULL)) {
            nlog_error("bind `%s` with description=`%s` fail: %s", query,
                       tag->description, sqlite3_errmsg(db));
            return -1;
        }

        if (SQLITE_OK != sqlite3_bind_null(stmt, 11)) {
            nlog_error("bind `%s` with value=null fail: %s", query,
                       sqlite3_errmsg(db));
            return -1;
        }

        if (SQLITE_OK != sqlite3_bind_text(stmt, 12, format_buf, -1, NULL)) {
            nlog_error("bind `%s` with format=`%s` fail: %s", query, format_buf,
                       sqlite3_errmsg(db));
            return -1;
        }

        if (SQLITE_DONE != sqlite3_step(stmt)) {
            nlog_error("sqlite3_step fail: %s", sqlite3_errmsg(db));
            return -1;
        }
    }

    return 0;
}

int neu_sqlite_persister_store_tags(neu_persister_t *    self,
                                    const char *         driver_name,
                                    const char *         group_name,
                                    const neu_datatag_t *tags, size_t n)
{
    neu_sqlite_persister_t *persister = (neu_sqlite_persister_t *) self;

    sqlite3_stmt *stmt = NULL;
    const char *  query =
        "INSERT INTO tags ("
        " driver_name, group_name, name, address, attribute,"
        " precision, type, decimal, bias, description, value, format"
        ") VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12)";

    if (SQLITE_OK != sqlite3_exec(persister->db, "BEGIN", NULL, NULL, NULL)) {
        nlog_error("begin transaction fail: %s", sqlite3_errmsg(persister->db));
        return NEU_ERR_EINTERNAL;
    }

    if (SQLITE_OK !=
        sqlite3_prepare_v2(persister->db, query, -1, &stmt, NULL)) {
        nlog_error("prepare `%s` fail: %s", query,
                   sqlite3_errmsg(persister->db));
        goto error;
    }

    if (SQLITE_OK != sqlite3_bind_text(stmt, 1, driver_name, -1, NULL)) {
        nlog_error("bind `%s` with driver_name=`%s` fail: %s", query,
                   driver_name, sqlite3_errmsg(persister->db));
        goto error;
    }

    if (SQLITE_OK != sqlite3_bind_text(stmt, 2, group_name, -1, NULL)) {
        nlog_error("bind `%s` with group_name=`%s` fail: %s", query, group_name,
                   sqlite3_errmsg(persister->db));
        goto error;
    }

    if (0 != put_tags(persister->db, query, stmt, tags, n)) {
        goto error;
    }

    if (SQLITE_OK != sqlite3_exec(persister->db, "COMMIT", NULL, NULL, NULL)) {
        nlog_error("commit transaction fail: %s",
                   sqlite3_errmsg(persister->db));
        goto error;
    }

    sqlite3_finalize(stmt);
    return 0;

error:
    nlog_warn("rollback transaction");
    sqlite3_exec(persister->db, "ROLLBACK", NULL, NULL, NULL);
    sqlite3_finalize(stmt);
    return NEU_ERR_EINTERNAL;
}

static int collect_tag_info(sqlite3_stmt *stmt, UT_array **tags)
{
    int step = sqlite3_step(stmt);
    while (SQLITE_ROW == step) {
        const char *format = (const char *) sqlite3_column_text(stmt, 9);

        neu_datatag_t tag = {
            .name        = (char *) sqlite3_column_text(stmt, 0),
            .address     = (char *) sqlite3_column_text(stmt, 1),
            .attribute   = sqlite3_column_int(stmt, 2),
            .precision   = sqlite3_column_int64(stmt, 3),
            .type        = sqlite3_column_int(stmt, 4),
            .decimal     = sqlite3_column_double(stmt, 5),
            .bias        = sqlite3_column_double(stmt, 6),
            .description = (char *) sqlite3_column_text(stmt, 7),
        };

        tag.n_format = neu_format_from_str(format, tag.format);

        utarray_push_back(*tags, &tag);

        step = sqlite3_step(stmt);
    }

    if (SQLITE_DONE != step) {
        return -1;
    }

    return 0;
}

int neu_sqlite_persister_load_tags(neu_persister_t *self,
                                   const char *     driver_name,
                                   const char *group_name, UT_array **tags)
{
    neu_sqlite_persister_t *persister = (neu_sqlite_persister_t *) self;

    sqlite3_stmt *stmt  = NULL;
    const char *  query = "SELECT name, address, attribute, precision, type, "
                        "decimal, bias, description, value, format "
                        "FROM tags WHERE driver_name=? AND group_name=? "
                        "ORDER BY rowid ASC";

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

    if (0 != collect_tag_info(stmt, tags)) {
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

int neu_sqlite_persister_update_tag(neu_persister_t *    self,
                                    const char *         driver_name,
                                    const char *         group_name,
                                    const neu_datatag_t *tag)
{
    int rv = execute_sql(((neu_sqlite_persister_t *) self)->db,
                         "UPDATE tags SET"
                         " address=%Q, attribute=%i, precision=%i, type=%i,"
                         " decimal=%lf, bias=%lf, description=%Q, value=%Q "
                         "WHERE driver_name=%Q AND group_name=%Q AND name=%Q",
                         tag->address, tag->attribute, tag->precision,
                         tag->type, tag->decimal, tag->bias, tag->description,
                         "", driver_name, group_name, tag->name);
    return rv;
}

int neu_sqlite_persister_update_tag_value(neu_persister_t *    self,
                                          const char *         driver_name,
                                          const char *         group_name,
                                          const neu_datatag_t *tag)
{
    int rv = execute_sql(((neu_sqlite_persister_t *) self)->db,
                         "UPDATE tags SET value=%Q "
                         "WHERE driver_name=%Q AND group_name=%Q AND name=%Q",
                         "", driver_name, group_name, tag->name);
    return rv;
}

int neu_sqlite_persister_delete_tag(neu_persister_t *self,
                                    const char *     driver_name,
                                    const char *     group_name,
                                    const char *     tag_name)
{
    int rv = execute_sql(
        ((neu_sqlite_persister_t *) self)->db,
        "DELETE FROM tags WHERE driver_name=%Q AND group_name=%Q AND name=%Q",
        driver_name, group_name, tag_name);
    return rv;
}

int neu_sqlite_persister_store_subscription(
    neu_persister_t *self, const char *app_name, const char *driver_name,
    const char *group_name, const char *params, const char *static_tags)
{
    return execute_sql(((neu_sqlite_persister_t *) self)->db,
                       "INSERT INTO subscriptions (app_name, driver_name, "
                       "group_name, params, static_tags) "
                       "VALUES (%Q, %Q, %Q, %Q, %Q)",
                       app_name, driver_name, group_name, params, static_tags);
}

int neu_sqlite_persister_update_subscription(
    neu_persister_t *self, const char *app_name, const char *driver_name,
    const char *group_name, const char *params, const char *static_tags)
{
    return execute_sql(((neu_sqlite_persister_t *) self)->db,
                       "UPDATE subscriptions SET params=%Q, static_tags=%Q "
                       "WHERE app_name=%Q AND driver_name=%Q AND group_name=%Q",
                       params, static_tags, app_name, driver_name, group_name);
}

static UT_icd subscription_info_icd = {
    sizeof(neu_persist_subscription_info_t),
    NULL,
    NULL,
    (dtor_f *) neu_persist_subscription_info_fini,
};

int neu_sqlite_persister_load_subscriptions(neu_persister_t *self,
                                            const char *     app_name,
                                            UT_array **      subscription_infos)
{
    neu_sqlite_persister_t *persister = (neu_sqlite_persister_t *) self;

    sqlite3_stmt *stmt  = NULL;
    const char *  query = "SELECT driver_name, group_name, params, static_tags "
                        "FROM subscriptions WHERE app_name=?";

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

        char *params = (char *) sqlite3_column_text(stmt, 2);
        // copy if params not NULL
        if (NULL != params && NULL == (params = strdup(params))) {
            free(group_name);
            free(driver_name);
            break;
        }

        char *static_tags = (char *) sqlite3_column_text(stmt, 3);
        // copy if params not NULL
        if (NULL != static_tags &&
            NULL == (static_tags = strdup(static_tags))) {
            free(group_name);
            free(driver_name);
            break;
        }

        neu_persist_subscription_info_t info = {
            .driver_name = driver_name,
            .group_name  = group_name,
            .params      = params,
            .static_tags = static_tags,
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

int neu_sqlite_persister_delete_subscription(neu_persister_t *self,
                                             const char *     app_name,
                                             const char *     driver_name,
                                             const char *     group_name)
{
    return execute_sql(((neu_sqlite_persister_t *) self)->db,
                       "DELETE FROM subscriptions WHERE app_name=%Q AND "
                       "driver_name=%Q AND group_name=%Q",
                       app_name, driver_name, group_name);
}

int neu_sqlite_persister_store_group(neu_persister_t *         self,
                                     const char *              driver_name,
                                     neu_persist_group_info_t *group_info,
                                     const char *              context)
{
    return execute_sql(((neu_sqlite_persister_t *) self)->db,
                       "INSERT INTO groups (driver_name, name, interval, "
                       "context) VALUES (%Q, %Q, %u, %Q)",
                       driver_name, group_info->name,
                       (unsigned) group_info->interval, context);
}

int neu_sqlite_persister_update_group(neu_persister_t *         self,
                                      const char *              driver_name,
                                      const char *              group_name,
                                      neu_persist_group_info_t *group_info)
{
    neu_sqlite_persister_t *persister = (neu_sqlite_persister_t *) self;

    int  ret             = -1;
    bool update_name     = (0 != strcmp(group_name, group_info->name));
    bool update_interval = (NEU_GROUP_INTERVAL_LIMIT <= group_info->interval);

    if (update_name && update_interval) {
        ret = execute_sql(persister->db,
                          "UPDATE groups SET name=%Q, interval=%i "
                          "WHERE driver_name=%Q AND name=%Q",
                          group_info->name, group_info->interval, driver_name,
                          group_name);
    } else if (update_name) {
        ret = execute_sql(persister->db,
                          "UPDATE groups SET name=%Q "
                          "WHERE driver_name=%Q AND name=%Q",
                          group_info->name, driver_name, group_name);
    } else if (update_interval) {
        ret = execute_sql(persister->db,
                          "UPDATE groups SET interval=%i "
                          "WHERE driver_name=%Q AND name=%Q",
                          group_info->interval, driver_name, group_name);
    }

    return ret;
}

static UT_icd group_info_icd = {
    sizeof(neu_persist_group_info_t),
    NULL,
    NULL,
    (dtor_f *) neu_persist_group_info_fini,
};

static int collect_group_info(sqlite3_stmt *stmt, UT_array **group_infos)
{
    int step = sqlite3_step(stmt);
    while (SQLITE_ROW == step) {
        neu_persist_group_info_t info = {};
        char *name = strdup((char *) sqlite3_column_text(stmt, 0));
        if (NULL == name) {
            break;
        }

        info.name     = name;
        info.interval = sqlite3_column_int(stmt, 1);
        char *context = (char *) sqlite3_column_text(stmt, 2);
        if (NULL != context) {
            info.context = strdup(context);
        } else {
            info.context = NULL;
        }
        utarray_push_back(*group_infos, &info);

        step = sqlite3_step(stmt);
    }

    if (SQLITE_DONE != step) {
        return -1;
    }

    return 0;
}

int neu_sqlite_persister_load_groups(neu_persister_t *self,
                                     const char *     driver_name,
                                     UT_array **      group_infos)
{
    neu_sqlite_persister_t *persister = (neu_sqlite_persister_t *) self;

    sqlite3_stmt *stmt = NULL;
    const char *  query =
        "SELECT name, interval, context FROM groups WHERE driver_name=?";

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

    if (0 != collect_group_info(stmt, group_infos)) {
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

int neu_sqlite_persister_delete_group(neu_persister_t *self,
                                      const char *     driver_name,
                                      const char *     group_name)
{
    // rely on foreign key constraints to delete tags and subscriptions
    int rv = execute_sql(((neu_sqlite_persister_t *) self)->db,
                         "DELETE FROM groups WHERE driver_name=%Q AND name=%Q",
                         driver_name, group_name);
    return rv;
}

int neu_sqlite_persister_store_node_setting(neu_persister_t *self,
                                            const char *     node_name,
                                            const char *     setting)
{
    return execute_sql(
        ((neu_sqlite_persister_t *) self)->db,
        "INSERT OR REPLACE INTO settings (node_name, setting) VALUES (%Q, %Q)",
        node_name, setting);
}

int neu_sqlite_persister_load_node_setting(neu_persister_t *  self,
                                           const char *       node_name,
                                           const char **const setting)
{
    neu_sqlite_persister_t *persister = (neu_sqlite_persister_t *) self;

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

int neu_sqlite_persister_delete_node_setting(neu_persister_t *self,
                                             const char *     node_name)
{
    return execute_sql(((neu_sqlite_persister_t *) self)->db,
                       "DELETE FROM settings WHERE node_name=%Q", node_name);
}

static UT_icd user_info_icd = {
    sizeof(neu_persist_user_info_t),
    NULL,
    NULL,
    (dtor_f *) neu_persist_user_info_fini,
};

static int collect_user_info(sqlite3_stmt *stmt, UT_array **user_infos)
{
    int step = sqlite3_step(stmt);
    while (SQLITE_ROW == step) {
        neu_persist_user_info_t info = {};
        char *name = strdup((char *) sqlite3_column_text(stmt, 0));
        if (NULL == name) {
            break;
        }

        info.name = name;
        info.hash = strdup((char *) sqlite3_column_text(stmt, 1));
        if (NULL == info.hash) {
            break;
        }
        utarray_push_back(*user_infos, &info);

        step = sqlite3_step(stmt);
    }

    if (SQLITE_DONE != step) {
        return -1;
    }

    return 0;
}

int neu_sqlite_persister_load_users(neu_persister_t *self,
                                    UT_array **      user_infos)
{
    neu_sqlite_persister_t *persister = (neu_sqlite_persister_t *) self;

    sqlite3_stmt *stmt  = NULL;
    const char *  query = "SELECT name, password FROM users";

    utarray_new(*user_infos, &user_info_icd);

    if (SQLITE_OK !=
        sqlite3_prepare_v2(persister->db, query, -1, &stmt, NULL)) {
        nlog_error("prepare `%s` fail: %s", query,
                   sqlite3_errmsg(persister->db));
        goto error;
    }

    if (0 != collect_user_info(stmt, user_infos)) {
        nlog_warn("query `%s` fail: %s", query, sqlite3_errmsg(persister->db));
        // do not set return code, return partial or empty result
    }

    sqlite3_finalize(stmt);
    return 0;

error:
    utarray_free(*user_infos);
    *user_infos = NULL;
    return NEU_ERR_EINTERNAL;
}

int neu_sqlite_persister_store_user(neu_persister_t *              self,
                                    const neu_persist_user_info_t *user)
{
    return execute_sql(((neu_sqlite_persister_t *) self)->db,
                       "INSERT INTO users (name, password) VALUES (%Q, %Q)",
                       user->name, user->hash);
}

int neu_sqlite_persister_update_user(neu_persister_t *              self,
                                     const neu_persist_user_info_t *user)
{
    return execute_sql(((neu_sqlite_persister_t *) self)->db,
                       "UPDATE users SET password=%Q WHERE name=%Q", user->hash,
                       user->name);
}

int neu_sqlite_persister_load_user(neu_persister_t *self, const char *user_name,
                                   neu_persist_user_info_t **user_p)
{
    neu_sqlite_persister_t *persister = (neu_sqlite_persister_t *) self;

    neu_persist_user_info_t *user  = NULL;
    sqlite3_stmt *           stmt  = NULL;
    const char *             query = "SELECT password FROM users WHERE name=?";

    if (SQLITE_OK !=
        sqlite3_prepare_v2(persister->db, query, -1, &stmt, NULL)) {
        nlog_error("prepare `%s` with `%s` fail: %s", query, user_name,
                   sqlite3_errmsg(persister->db));
        return NEU_ERR_EINTERNAL;
    }

    if (SQLITE_OK != sqlite3_bind_text(stmt, 1, user_name, -1, NULL)) {
        nlog_error("bind `%s` with `%s` fail: %s", query, user_name,
                   sqlite3_errmsg(persister->db));
        goto error;
    }

    if (SQLITE_ROW != sqlite3_step(stmt)) {
        nlog_warn("SQL `%s` with `%s` fail: %s", query, user_name,
                  sqlite3_errmsg(persister->db));
        goto error;
    }

    user = calloc(1, sizeof(*user));
    if (NULL == user) {
        goto error;
    }

    user->hash = strdup((char *) sqlite3_column_text(stmt, 0));
    if (NULL == user->hash) {
        nlog_error("strdup fail");
        goto error;
    }

    user->name = strdup(user_name);
    if (NULL == user->name) {
        nlog_error("strdup fail");
        goto error;
    }

    *user_p = user;
    sqlite3_finalize(stmt);
    return 0;

error:
    if (user) {
        neu_persist_user_info_fini(user);
        free(user);
    }
    sqlite3_finalize(stmt);
    return NEU_ERR_EINTERNAL;
}

int neu_sqlite_persister_delete_user(neu_persister_t *self,
                                     const char *     user_name)
{
    return execute_sql(((neu_sqlite_persister_t *) self)->db,
                       "DELETE FROM users WHERE name=%Q", user_name);
}

static UT_icd client_cert_info_icd = {
    sizeof(neu_persist_client_cert_info_t),
    NULL,
    NULL,
    (dtor_f *) neu_persist_client_cert_info_fini,
};

int neu_sqlite_persister_store_server_cert(
    neu_persister_t *self, const neu_persist_server_cert_info_t *cert_info)
{
    neu_sqlite_persister_t *persister = (neu_sqlite_persister_t *) self;
    sqlite3_stmt *          stmt      = NULL;
    const char *            query =
        "INSERT OR REPLACE INTO server_certificates "
        "(app_name, common_name, subject, issuer, valid_from, valid_to, "
        "serial_number, fingerprint, certificate_data, private_key_data) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

    if (SQLITE_OK !=
        sqlite3_prepare_v2(persister->db, query, -1, &stmt, NULL)) {
        nlog_error("prepare `%s` fail: %s", query,
                   sqlite3_errmsg(persister->db));
        return NEU_ERR_EINTERNAL;
    }

    sqlite3_bind_text(stmt, 1, cert_info->app_name, -1, NULL);
    sqlite3_bind_text(stmt, 2, cert_info->common_name, -1, NULL);
    sqlite3_bind_text(stmt, 3, cert_info->subject, -1, NULL);
    sqlite3_bind_text(stmt, 4, cert_info->issuer, -1, NULL);
    sqlite3_bind_text(stmt, 5, cert_info->valid_from, -1, NULL);
    sqlite3_bind_text(stmt, 6, cert_info->valid_to, -1, NULL);
    sqlite3_bind_text(stmt, 7, cert_info->serial_number, -1, NULL);
    sqlite3_bind_text(stmt, 8, cert_info->fingerprint, -1, NULL);

    if (cert_info->certificate_data && cert_info->certificate_size > 0) {
        sqlite3_bind_blob(stmt, 9, cert_info->certificate_data,
                          cert_info->certificate_size, NULL);
    } else {
        sqlite3_bind_null(stmt, 9);
    }

    if (cert_info->private_key_data && cert_info->private_key_size > 0) {
        sqlite3_bind_blob(stmt, 10, cert_info->private_key_data,
                          cert_info->private_key_size, NULL);
    } else {
        sqlite3_bind_null(stmt, 10);
    }

    int rv = 0;
    if (SQLITE_DONE != sqlite3_step(stmt)) {
        nlog_error("execute `%s` fail: %s", query,
                   sqlite3_errmsg(persister->db));
        rv = NEU_ERR_EINTERNAL;
    }

    sqlite3_finalize(stmt);
    return rv;
}

int neu_sqlite_persister_update_server_cert(
    neu_persister_t *self, const neu_persist_server_cert_info_t *cert_info)
{
    // For server certificates, each app can only have one record, so update and
    // insert are the same operation
    return neu_sqlite_persister_store_server_cert(self, cert_info);
}

int neu_sqlite_persister_load_server_cert(
    neu_persister_t *self, const char *app_name,
    neu_persist_server_cert_info_t **cert_info_p)
{
    neu_sqlite_persister_t *persister = (neu_sqlite_persister_t *) self;
    sqlite3_stmt *          stmt      = NULL;
    const char *            query =
        "SELECT id, app_name, common_name, subject, issuer, valid_from, "
        "valid_to, "
        "serial_number, fingerprint, certificate_data, private_key_data, "
        "created_at, updated_at FROM server_certificates WHERE app_name=?";

    if (SQLITE_OK !=
        sqlite3_prepare_v2(persister->db, query, -1, &stmt, NULL)) {
        nlog_error("prepare `%s` fail: %s", query,
                   sqlite3_errmsg(persister->db));
        return NEU_ERR_EINTERNAL;
    }

    if (SQLITE_OK != sqlite3_bind_text(stmt, 1, app_name, -1, NULL)) {
        nlog_error("bind `%s` with `%s` fail: %s", query, app_name,
                   sqlite3_errmsg(persister->db));
        sqlite3_finalize(stmt);
        return NEU_ERR_EINTERNAL;
    }

    neu_persist_server_cert_info_t *cert_info = NULL;
    if (SQLITE_ROW != sqlite3_step(stmt)) {
        sqlite3_finalize(stmt);
        return NEU_ERR_EINTERNAL;
    }

    cert_info = calloc(1, sizeof(*cert_info));
    if (NULL == cert_info) {
        sqlite3_finalize(stmt);
        return NEU_ERR_EINTERNAL;
    }

    // Read all fields
    cert_info->id          = sqlite3_column_int(stmt, 0);
    cert_info->app_name    = strdup((char *) sqlite3_column_text(stmt, 1));
    cert_info->common_name = strdup((char *) sqlite3_column_text(stmt, 2));

    char *subject      = (char *) sqlite3_column_text(stmt, 3);
    cert_info->subject = subject ? strdup(subject) : NULL;

    char *issuer      = (char *) sqlite3_column_text(stmt, 4);
    cert_info->issuer = issuer ? strdup(issuer) : NULL;

    char *valid_from      = (char *) sqlite3_column_text(stmt, 5);
    cert_info->valid_from = valid_from ? strdup(valid_from) : NULL;

    char *valid_to      = (char *) sqlite3_column_text(stmt, 6);
    cert_info->valid_to = valid_to ? strdup(valid_to) : NULL;

    char *serial_number      = (char *) sqlite3_column_text(stmt, 7);
    cert_info->serial_number = serial_number ? strdup(serial_number) : NULL;

    char *fingerprint      = (char *) sqlite3_column_text(stmt, 8);
    cert_info->fingerprint = fingerprint ? strdup(fingerprint) : NULL;

    // Read certificate data
    const void *cert_data       = sqlite3_column_blob(stmt, 9);
    cert_info->certificate_size = sqlite3_column_bytes(stmt, 9);
    if (cert_data && cert_info->certificate_size > 0) {
        cert_info->certificate_data = malloc(cert_info->certificate_size);
        if (cert_info->certificate_data) {
            memcpy(cert_info->certificate_data, cert_data,
                   cert_info->certificate_size);
        }
    }

    // Read private key data
    const void *key_data        = sqlite3_column_blob(stmt, 10);
    cert_info->private_key_size = sqlite3_column_bytes(stmt, 10);
    if (key_data && cert_info->private_key_size > 0) {
        cert_info->private_key_data = malloc(cert_info->private_key_size);
        if (cert_info->private_key_data) {
            memcpy(cert_info->private_key_data, key_data,
                   cert_info->private_key_size);
        }
    }

    char *created_at      = (char *) sqlite3_column_text(stmt, 11);
    cert_info->created_at = created_at ? strdup(created_at) : NULL;

    char *updated_at      = (char *) sqlite3_column_text(stmt, 12);
    cert_info->updated_at = updated_at ? strdup(updated_at) : NULL;

    *cert_info_p = cert_info;
    sqlite3_finalize(stmt);
    return 0;
}

static int collect_client_cert_info(sqlite3_stmt *stmt, UT_array **cert_infos)
{
    int step = sqlite3_step(stmt);
    while (SQLITE_ROW == step) {
        neu_persist_client_cert_info_t info = {};

        info.id = sqlite3_column_int(stmt, 0);

        char *app_name = strdup((char *) sqlite3_column_text(stmt, 1));
        if (NULL == app_name) {
            break;
        }
        info.app_name = app_name;

        char *common_name = strdup((char *) sqlite3_column_text(stmt, 2));
        if (NULL == common_name) {
            free(app_name);
            break;
        }
        info.common_name = common_name;

        char *subject = (char *) sqlite3_column_text(stmt, 3);
        if (subject != NULL) {
            info.subject = strdup(subject);
        } else {
            info.subject = NULL;
        }

        char *issuer = (char *) sqlite3_column_text(stmt, 4);
        if (issuer != NULL) {
            info.issuer = strdup(issuer);
        } else {
            info.issuer = NULL;
        }

        char *valid_from = (char *) sqlite3_column_text(stmt, 5);
        if (valid_from != NULL) {
            info.valid_from = strdup(valid_from);
        } else {
            info.valid_from = NULL;
        }

        char *valid_to = (char *) sqlite3_column_text(stmt, 6);
        if (valid_to != NULL) {
            info.valid_to = strdup(valid_to);
        } else {
            info.valid_to = NULL;
        }

        char *serial_number = (char *) sqlite3_column_text(stmt, 7);
        if (serial_number != NULL) {
            info.serial_number = strdup(serial_number);
        } else {
            info.serial_number = NULL;
        }

        char *fingerprint = (char *) sqlite3_column_text(stmt, 8);
        if (fingerprint != NULL) {
            info.fingerprint = strdup(fingerprint);
        } else {
            info.fingerprint = NULL;
        }

        // Handle certificate data (BLOB)
        const void *cert_data = sqlite3_column_blob(stmt, 9);
        info.certificate_size = sqlite3_column_bytes(stmt, 9);
        if (cert_data != NULL && info.certificate_size > 0) {
            info.certificate_data = malloc(info.certificate_size);
            if (info.certificate_data != NULL) {
                memcpy(info.certificate_data, cert_data, info.certificate_size);
            }
        } else {
            info.certificate_data = NULL;
            info.certificate_size = 0;
        }

        info.is_ca        = sqlite3_column_int(stmt, 10);
        info.trust_status = sqlite3_column_int(stmt, 11);

        char *created_at = (char *) sqlite3_column_text(stmt, 12);
        if (created_at != NULL) {
            info.created_at = strdup(created_at);
        } else {
            info.created_at = NULL;
        }

        utarray_push_back(*cert_infos, &info);
        step = sqlite3_step(stmt);
    }

    if (SQLITE_DONE != step) {
        return -1;
    }

    return 0;
}

int neu_sqlite_persister_store_client_cert(
    neu_persister_t *self, const neu_persist_client_cert_info_t *cert_info)
{
    neu_sqlite_persister_t *persister = (neu_sqlite_persister_t *) self;
    sqlite3_stmt *          stmt      = NULL;
    const char *            sql =
        "INSERT INTO client_certificates "
        "(app_name, common_name, subject, issuer, valid_from, valid_to, "
        "serial_number, fingerprint, certificate_data, is_ca, trust_status) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

    if (SQLITE_OK != sqlite3_prepare_v2(persister->db, sql, -1, &stmt, NULL)) {
        nlog_error("prepare `%s` fail: %s", sql, sqlite3_errmsg(persister->db));
        return NEU_ERR_EINTERNAL;
    }

    sqlite3_bind_text(stmt, 1, cert_info->app_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, cert_info->common_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, cert_info->subject, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, cert_info->issuer, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, cert_info->valid_from, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, cert_info->valid_to, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 7, cert_info->serial_number, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 8, cert_info->fingerprint, -1, SQLITE_STATIC);

    if (cert_info->certificate_data && cert_info->certificate_size > 0) {
        sqlite3_bind_blob(stmt, 9, cert_info->certificate_data,
                          cert_info->certificate_size, SQLITE_STATIC);
    } else {
        sqlite3_bind_null(stmt, 9);
    }

    sqlite3_bind_int(stmt, 10, cert_info->is_ca);
    sqlite3_bind_int(stmt, 11, cert_info->trust_status);

    int ret = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (SQLITE_DONE != ret) {
        nlog_error("execute `%s` fail: %s", sql, sqlite3_errmsg(persister->db));
        return NEU_ERR_EINTERNAL;
    }

    return 0;
}

int neu_sqlite_persister_update_client_cert(
    neu_persister_t *self, const neu_persist_client_cert_info_t *cert_info)
{
    neu_sqlite_persister_t *persister = (neu_sqlite_persister_t *) self;
    sqlite3_stmt *          stmt      = NULL;
    const char *            sql =
        "UPDATE client_certificates SET trust_status=? WHERE fingerprint=?";

    if (SQLITE_OK != sqlite3_prepare_v2(persister->db, sql, -1, &stmt, NULL)) {
        nlog_error("prepare `%s` fail: %s", sql, sqlite3_errmsg(persister->db));
        return NEU_ERR_EINTERNAL;
    }

    sqlite3_bind_int(stmt, 1, cert_info->trust_status);
    sqlite3_bind_text(stmt, 2, cert_info->fingerprint, -1, SQLITE_STATIC);

    int ret = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (SQLITE_DONE != ret) {
        nlog_error("execute `%s` fail: %s", sql, sqlite3_errmsg(persister->db));
        return NEU_ERR_EINTERNAL;
    }

    return 0;
}

int neu_sqlite_persister_load_client_certs_by_app(neu_persister_t *self,
                                                  const char *     app_name,
                                                  UT_array **      cert_infos)
{
    neu_sqlite_persister_t *persister = (neu_sqlite_persister_t *) self;
    sqlite3_stmt *          stmt      = NULL;
    const char *            query =
        "SELECT id, app_name, common_name, subject, issuer, valid_from, "
        "valid_to, serial_number, fingerprint, certificate_data, is_ca, "
        "trust_status, created_at FROM client_certificates WHERE app_name=? "
        "ORDER BY id";

    utarray_new(*cert_infos, &client_cert_info_icd);

    if (SQLITE_OK !=
        sqlite3_prepare_v2(persister->db, query, -1, &stmt, NULL)) {
        nlog_error("prepare `%s` fail: %s", query,
                   sqlite3_errmsg(persister->db));
        goto error;
    }

    sqlite3_bind_text(stmt, 1, app_name, -1, SQLITE_STATIC);

    if (0 != collect_client_cert_info(stmt, cert_infos)) {
        nlog_warn("query `%s` fail: %s", query, sqlite3_errmsg(persister->db));
    }

    sqlite3_finalize(stmt);
    return 0;

error:
    utarray_free(*cert_infos);
    *cert_infos = NULL;
    return NEU_ERR_EINTERNAL;
}

int neu_sqlite_persister_load_client_certs(neu_persister_t *self,
                                           UT_array **      cert_infos)
{
    neu_sqlite_persister_t *persister = (neu_sqlite_persister_t *) self;
    sqlite3_stmt *          stmt      = NULL;
    const char *            query =
        "SELECT id, app_name, common_name, subject, issuer, valid_from, "
        "valid_to, serial_number, fingerprint, certificate_data, is_ca, "
        "trust_status, created_at FROM client_certificates ORDER BY app_name, "
        "id";

    utarray_new(*cert_infos, &client_cert_info_icd);

    if (SQLITE_OK !=
        sqlite3_prepare_v2(persister->db, query, -1, &stmt, NULL)) {
        nlog_error("prepare `%s` fail: %s", query,
                   sqlite3_errmsg(persister->db));
        goto error;
    }

    if (0 != collect_client_cert_info(stmt, cert_infos)) {
        nlog_warn("query `%s` fail: %s", query, sqlite3_errmsg(persister->db));
    }

    sqlite3_finalize(stmt);
    return 0;

error:
    utarray_free(*cert_infos);
    *cert_infos = NULL;
    return NEU_ERR_EINTERNAL;
}

int neu_sqlite_persister_delete_client_cert(neu_persister_t *self,
                                            const char *     fingerprint)
{
    return execute_sql(((neu_sqlite_persister_t *) self)->db,
                       "DELETE FROM client_certificates WHERE fingerprint=%Q",
                       fingerprint);
}

// Security policy operations implementation
int neu_sqlite_persister_store_security_policy(
    neu_persister_t *                         self,
    const neu_persist_security_policy_info_t *policy_info)
{
    return execute_sql(
        ((neu_sqlite_persister_t *) self)->db,
        "INSERT OR REPLACE INTO security_policies (app_name, policy_name) "
        "VALUES (%Q, %Q)",
        policy_info->app_name, policy_info->policy_name);
}

int neu_sqlite_persister_update_security_policy(
    neu_persister_t *                         self,
    const neu_persist_security_policy_info_t *policy_info)
{
    // For security policies, each app can only have one policy record,
    // so update and insert are the same operation
    return neu_sqlite_persister_store_security_policy(self, policy_info);
}

int neu_sqlite_persister_load_security_policy(
    neu_persister_t *self, const char *app_name,
    neu_persist_security_policy_info_t **policy_info_p)
{
    neu_sqlite_persister_t *persister = (neu_sqlite_persister_t *) self;
    sqlite3_stmt *          stmt      = NULL;
    const char *            query =
        "SELECT id, app_name, policy_name, created_at, updated_at "
        "FROM security_policies WHERE app_name=?";

    if (SQLITE_OK !=
        sqlite3_prepare_v2(persister->db, query, -1, &stmt, NULL)) {
        nlog_error("prepare `%s` with `%s` fail: %s", query, app_name,
                   sqlite3_errmsg(persister->db));
        return NEU_ERR_EINTERNAL;
    }

    if (SQLITE_OK != sqlite3_bind_text(stmt, 1, app_name, -1, NULL)) {
        nlog_error("bind `%s` with `%s` fail: %s", query, app_name,
                   sqlite3_errmsg(persister->db));
        sqlite3_finalize(stmt);
        return NEU_ERR_EINTERNAL;
    }

    int step = sqlite3_step(stmt);
    if (SQLITE_ROW == step) {
        neu_persist_security_policy_info_t *info =
            calloc(1, sizeof(neu_persist_security_policy_info_t));
        if (NULL == info) {
            sqlite3_finalize(stmt);
            return NEU_ERR_EINTERNAL;
        }

        info->id = sqlite3_column_int(stmt, 0);

        char *app_name_col = strdup((char *) sqlite3_column_text(stmt, 1));
        if (NULL == app_name_col) {
            free(info);
            sqlite3_finalize(stmt);
            return NEU_ERR_EINTERNAL;
        }
        info->app_name = app_name_col;

        char *policy_name = strdup((char *) sqlite3_column_text(stmt, 2));
        if (NULL == policy_name) {
            free(info->app_name);
            free(info);
            sqlite3_finalize(stmt);
            return NEU_ERR_EINTERNAL;
        }
        info->policy_name = policy_name;

        char *created_at = (char *) sqlite3_column_text(stmt, 3);
        if (created_at != NULL) {
            info->created_at = strdup(created_at);
        } else {
            info->created_at = NULL;
        }

        char *updated_at = (char *) sqlite3_column_text(stmt, 4);
        if (updated_at != NULL) {
            info->updated_at = strdup(updated_at);
        } else {
            info->updated_at = NULL;
        }

        *policy_info_p = info;
        sqlite3_finalize(stmt);
        return 0;
    } else if (SQLITE_DONE == step) {
        // No policy found for this app
        sqlite3_finalize(stmt);
        return NEU_ERR_NODE_NOT_EXIST;
    } else {
        nlog_warn("query `%s` fail: %s", query, sqlite3_errmsg(persister->db));
        sqlite3_finalize(stmt);
        return NEU_ERR_EINTERNAL;
    }
}

static UT_icd security_policy_info_icd = {
    sizeof(neu_persist_security_policy_info_t),
    NULL,
    NULL,
    (dtor_f *) neu_persist_security_policy_info_fini,
};

static int collect_security_policy_info(sqlite3_stmt *stmt,
                                        UT_array **   policy_infos)
{
    int step = sqlite3_step(stmt);
    while (SQLITE_ROW == step) {
        neu_persist_security_policy_info_t info = {};

        info.id = sqlite3_column_int(stmt, 0);

        char *app_name = strdup((char *) sqlite3_column_text(stmt, 1));
        if (NULL == app_name) {
            break;
        }
        info.app_name = app_name;

        char *policy_name = strdup((char *) sqlite3_column_text(stmt, 2));
        if (NULL == policy_name) {
            free(app_name);
            break;
        }
        info.policy_name = policy_name;

        char *created_at = (char *) sqlite3_column_text(stmt, 3);
        if (created_at != NULL) {
            info.created_at = strdup(created_at);
        } else {
            info.created_at = NULL;
        }

        char *updated_at = (char *) sqlite3_column_text(stmt, 4);
        if (updated_at != NULL) {
            info.updated_at = strdup(updated_at);
        } else {
            info.updated_at = NULL;
        }

        utarray_push_back(*policy_infos, &info);
        step = sqlite3_step(stmt);
    }

    return step;
}

int neu_sqlite_persister_load_security_policies(neu_persister_t *self,
                                                UT_array **      policy_infos)
{
    neu_sqlite_persister_t *persister = (neu_sqlite_persister_t *) self;
    sqlite3_stmt *          stmt      = NULL;
    const char *            query =
        "SELECT id, app_name, policy_name, created_at, updated_at "
        "FROM security_policies ORDER BY app_name";

    utarray_new(*policy_infos, &security_policy_info_icd);

    if (SQLITE_OK !=
        sqlite3_prepare_v2(persister->db, query, -1, &stmt, NULL)) {
        nlog_error("prepare `%s` fail: %s", query,
                   sqlite3_errmsg(persister->db));
        goto error;
    }

    if (0 != collect_security_policy_info(stmt, policy_infos)) {
        nlog_warn("query `%s` fail: %s", query, sqlite3_errmsg(persister->db));
        // do not set return code, return partial or empty result
    }

    sqlite3_finalize(stmt);
    return 0;

error:
    utarray_free(*policy_infos);
    *policy_infos = NULL;
    return NEU_ERR_EINTERNAL;
}

static UT_icd auth_user_info_icd = {
    sizeof(neu_persist_auth_user_info_t),
    NULL,
    NULL,
    (dtor_f *) neu_persist_auth_user_info_fini,
};

int neu_sqlite_persister_store_auth_setting(
    neu_persister_t *self, const neu_persist_auth_setting_info_t *auth_info)
{
    return execute_sql(
        ((neu_sqlite_persister_t *) self)->db,
        "INSERT INTO auth_settings (app_name, enabled) VALUES (%Q, %d)",
        auth_info->app_name, auth_info->enabled);
}

int neu_sqlite_persister_update_auth_setting(
    neu_persister_t *self, const neu_persist_auth_setting_info_t *auth_info)
{
    return execute_sql(((neu_sqlite_persister_t *) self)->db,
                       "UPDATE auth_settings SET enabled=%d WHERE app_name=%Q",
                       auth_info->enabled, auth_info->app_name);
}

int neu_sqlite_persister_load_auth_setting(
    neu_persister_t *self, const char *app_name,
    neu_persist_auth_setting_info_t **auth_info_p)
{
    neu_sqlite_persister_t *persister = (neu_sqlite_persister_t *) self;

    neu_persist_auth_setting_info_t *auth_info = NULL;
    sqlite3_stmt *                   stmt      = NULL;
    const char *query = "SELECT id, app_name, enabled, created_at, updated_at "
                        "FROM auth_settings WHERE app_name=?";

    if (SQLITE_OK !=
        sqlite3_prepare_v2(persister->db, query, -1, &stmt, NULL)) {
        nlog_error("prepare `%s` with `%s` fail: %s", query, app_name,
                   sqlite3_errmsg(persister->db));
        return NEU_ERR_EINTERNAL;
    }

    if (SQLITE_OK != sqlite3_bind_text(stmt, 1, app_name, -1, NULL)) {
        nlog_error("bind `%s` with `%s` fail: %s", query, app_name,
                   sqlite3_errmsg(persister->db));
        goto error;
    }

    if (SQLITE_ROW != sqlite3_step(stmt)) {
        nlog_warn("SQL `%s` with `%s` fail: %s", query, app_name,
                  sqlite3_errmsg(persister->db));
        goto error;
    }

    auth_info = calloc(1, sizeof(*auth_info));
    if (NULL == auth_info) {
        goto error;
    }

    auth_info->id      = sqlite3_column_int(stmt, 0);
    auth_info->enabled = sqlite3_column_int(stmt, 2);

    auth_info->app_name = strdup((char *) sqlite3_column_text(stmt, 1));
    if (NULL == auth_info->app_name) {
        nlog_error("strdup fail");
        goto error;
    }

    char *created_at = (char *) sqlite3_column_text(stmt, 3);
    if (created_at != NULL) {
        auth_info->created_at = strdup(created_at);
    }

    char *updated_at = (char *) sqlite3_column_text(stmt, 4);
    if (updated_at != NULL) {
        auth_info->updated_at = strdup(updated_at);
    }

    sqlite3_finalize(stmt);
    *auth_info_p = auth_info;
    return 0;

error:
    if (auth_info) {
        neu_persist_auth_setting_info_fini(auth_info);
        free(auth_info);
    }
    sqlite3_finalize(stmt);
    return NEU_ERR_EINTERNAL;
}

static int collect_auth_user_info(sqlite3_stmt *stmt, UT_array **user_infos)
{
    int step = sqlite3_step(stmt);
    while (SQLITE_ROW == step) {
        neu_persist_auth_user_info_t info = {};

        info.id = sqlite3_column_int(stmt, 0);

        char *app_name = strdup((char *) sqlite3_column_text(stmt, 1));
        if (NULL == app_name) {
            break;
        }
        info.app_name = app_name;

        char *username = strdup((char *) sqlite3_column_text(stmt, 2));
        if (NULL == username) {
            free(app_name);
            break;
        }
        info.username = username;

        char *password_hash = strdup((char *) sqlite3_column_text(stmt, 3));
        if (NULL == password_hash) {
            free(app_name);
            free(username);
            break;
        }
        info.password_hash = password_hash;

        char *created_at = (char *) sqlite3_column_text(stmt, 4);
        if (created_at != NULL) {
            info.created_at = strdup(created_at);
        } else {
            info.created_at = NULL;
        }

        char *updated_at = (char *) sqlite3_column_text(stmt, 5);
        if (updated_at != NULL) {
            info.updated_at = strdup(updated_at);
        } else {
            info.updated_at = NULL;
        }

        utarray_push_back(*user_infos, &info);
        step = sqlite3_step(stmt);
    }

    return step;
}

int neu_sqlite_persister_store_auth_user(
    neu_persister_t *self, const neu_persist_auth_user_info_t *user_info)
{
    return execute_sql(
        ((neu_sqlite_persister_t *) self)->db,
        "INSERT INTO auth_users (app_name, username, password_hash) "
        "VALUES (%Q, %Q, %Q)",
        user_info->app_name, user_info->username, user_info->password_hash);
}

int neu_sqlite_persister_update_auth_user(
    neu_persister_t *self, const neu_persist_auth_user_info_t *user_info)
{
    return execute_sql(((neu_sqlite_persister_t *) self)->db,
                       "UPDATE auth_users SET password_hash=%Q "
                       "WHERE app_name=%Q AND username=%Q",
                       user_info->password_hash, user_info->app_name,
                       user_info->username);
}

int neu_sqlite_persister_load_auth_user(
    neu_persister_t *self, const char *app_name, const char *username,
    neu_persist_auth_user_info_t **user_info_p)
{
    neu_sqlite_persister_t *persister = (neu_sqlite_persister_t *) self;

    neu_persist_auth_user_info_t *user_info = NULL;
    sqlite3_stmt *                stmt      = NULL;
    const char *                  query =
        "SELECT id, app_name, username, password_hash, created_at, updated_at "
        "FROM auth_users WHERE app_name=? AND username=?";

    if (SQLITE_OK !=
        sqlite3_prepare_v2(persister->db, query, -1, &stmt, NULL)) {
        nlog_error("prepare `%s` with `%s:%s` fail: %s", query, app_name,
                   username, sqlite3_errmsg(persister->db));
        return NEU_ERR_EINTERNAL;
    }

    if (SQLITE_OK != sqlite3_bind_text(stmt, 1, app_name, -1, NULL)) {
        nlog_error("bind `%s` with `%s` fail: %s", query, app_name,
                   sqlite3_errmsg(persister->db));
        goto error;
    }

    if (SQLITE_OK != sqlite3_bind_text(stmt, 2, username, -1, NULL)) {
        nlog_error("bind `%s` with `%s` fail: %s", query, username,
                   sqlite3_errmsg(persister->db));
        goto error;
    }

    if (SQLITE_ROW != sqlite3_step(stmt)) {
        nlog_warn("SQL `%s` with `%s:%s` fail: %s", query, app_name, username,
                  sqlite3_errmsg(persister->db));
        goto error;
    }

    user_info = calloc(1, sizeof(*user_info));
    if (NULL == user_info) {
        goto error;
    }

    user_info->id = sqlite3_column_int(stmt, 0);

    user_info->app_name = strdup((char *) sqlite3_column_text(stmt, 1));
    if (NULL == user_info->app_name) {
        nlog_error("strdup fail");
        goto error;
    }

    user_info->username = strdup((char *) sqlite3_column_text(stmt, 2));
    if (NULL == user_info->username) {
        nlog_error("strdup fail");
        goto error;
    }

    user_info->password_hash = strdup((char *) sqlite3_column_text(stmt, 3));
    if (NULL == user_info->password_hash) {
        nlog_error("strdup fail");
        goto error;
    }

    char *created_at = (char *) sqlite3_column_text(stmt, 4);
    if (created_at != NULL) {
        user_info->created_at = strdup(created_at);
    }

    char *updated_at = (char *) sqlite3_column_text(stmt, 5);
    if (updated_at != NULL) {
        user_info->updated_at = strdup(updated_at);
    }

    sqlite3_finalize(stmt);
    *user_info_p = user_info;
    return 0;

error:
    if (user_info) {
        neu_persist_auth_user_info_fini(user_info);
        free(user_info);
    }
    sqlite3_finalize(stmt);
    return NEU_ERR_EINTERNAL;
}

int neu_sqlite_persister_load_auth_users_by_app(neu_persister_t *self,
                                                const char *     app_name,
                                                UT_array **      user_infos)
{
    neu_sqlite_persister_t *persister = (neu_sqlite_persister_t *) self;
    sqlite3_stmt *          stmt      = NULL;
    const char *            query =
        "SELECT id, app_name, username, password_hash, created_at, updated_at "
        "FROM auth_users WHERE app_name=? ORDER BY username";

    utarray_new(*user_infos, &auth_user_info_icd);

    if (SQLITE_OK !=
        sqlite3_prepare_v2(persister->db, query, -1, &stmt, NULL)) {
        nlog_error("prepare `%s` with `%s` fail: %s", query, app_name,
                   sqlite3_errmsg(persister->db));
        goto error;
    }

    if (SQLITE_OK != sqlite3_bind_text(stmt, 1, app_name, -1, NULL)) {
        nlog_error("bind `%s` with `%s` fail: %s", query, app_name,
                   sqlite3_errmsg(persister->db));
        goto error;
    }

    if (0 != collect_auth_user_info(stmt, user_infos)) {
        nlog_warn("query `%s` with `%s` fail: %s", query, app_name,
                  sqlite3_errmsg(persister->db));
        // do not set return code, return partial or empty result
    }

    sqlite3_finalize(stmt);
    return 0;

error:
    utarray_free(*user_infos);
    *user_infos = NULL;
    sqlite3_finalize(stmt);
    return NEU_ERR_EINTERNAL;
}

int neu_sqlite_persister_delete_auth_user(neu_persister_t *self,
                                          const char *     app_name,
                                          const char *     username)
{
    return execute_sql(
        ((neu_sqlite_persister_t *) self)->db,
        "DELETE FROM auth_users WHERE app_name=%Q AND username=%Q", app_name,
        username);
}
