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

#include <sqlite3.h>

#include "errcodes.h"
#include "utils/asprintf.h"
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

static const char *plugin_file       = "persistence/plugins.json";
static const char *db_file           = "persistence/sqlite.db";
static sqlite3 *   global_db         = NULL;
pthread_rwlock_t   global_rwlock     = PTHREAD_RWLOCK_INITIALIZER;
static int         global_node_count = 0;
static int         global_tag_count  = 0;

/**
 * 检查字符串是否以指定后缀结尾
 *
 * @param str     要检查的字符串
 * @param suffix  后缀字符串
 *
 * @return 如果字符串以指定后缀结尾，返回true，否则返回false
 */
static inline bool ends_with(const char *str, const char *suffix)
{
    size_t m = strlen(str);
    size_t n = strlen(suffix);
    return m >= n && !strcmp(str + m - n, suffix);
}

/**
 * 连接路径字符串
 *
 * @param dst   目标路径字符串缓冲区
 * @param len   目标路径字符串长度，不大于缓冲区大小
 * @param size  目标路径缓冲区大小
 * @param src   源路径字符串
 *
 * @return 结果路径字符串的长度（不包括终止NULL字节），如果溢出则返回size
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
 * 将字符串写入文件
 *
 * @param fn  文件名
 * @param s   要写入的字符串
 *
 * @return 成功返回0，失败返回-1
 */
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

/**
 * 读取文件内容为字符串
 *
 * @param fn   文件名
 * @param out  输出参数，保存读取到的字符串
 *
 * @return 成功返回0，失败返回-1
 */
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

/**
 * 执行SQL语句
 *
 * @param db   数据库连接
 * @param sql  SQL语句格式字符串
 * @param ...  格式化参数
 *
 * @return 成功返回0，失败返回错误码
 */
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

/**
 * 获取数据库架构版本
 *
 * @param db        数据库连接
 * @param version_p 输出参数，保存版本字符串
 * @param dirty_p   输出参数，表示数据库是否为脏状态
 *
 * @return 成功返回0，失败返回错误码
 */
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

/**
 * 比较两个架构版本字符串
 *
 * @param a 第一个版本字符串指针
 * @param b 第二个版本字符串指针
 *
 * @return 比较结果
 */
int schema_version_cmp(const void *a, const void *b)
{
    return strcmp(*(char **) a, *(char **) b);
}

/**
 * 从文件名中提取架构信息
 *
 * @param file          文件名
 * @param version_p     输出参数，保存版本字符串
 * @param description_p 输出参数，保存描述字符串
 *
 * @return 成功返回0，失败返回错误码
 */
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

/**
 * 检查架构版本是否应该应用
 *
 * @param db      数据库连接
 * @param version 版本字符串
 *
 * @return 1表示应该应用，0表示不应该应用，-1表示出错
 */
static int should_apply(sqlite3 *db, const char *version)
{
    int           rv   = 0;
    sqlite3_stmt *stmt = NULL;
    const char *  sql  = "SELECT count(*) FROM migrations WHERE version=?";

    if (SQLITE_OK != sqlite3_prepare_v2(db, sql, -1, &stmt, NULL)) {
        nlog_error("prepare `%s` fail: %s", sql, sqlite3_errmsg(global_db));
        rv = -1;
        goto end;
    }

    if (SQLITE_OK != sqlite3_bind_text(stmt, 1, version, -1, NULL)) {
        nlog_error("bind `%s` with version=`%s` fail: %s", sql, version,
                   sqlite3_errmsg(global_db));
        rv = -1;
        goto end;
    }

    if (SQLITE_ROW == sqlite3_step(stmt)) {
        rv = sqlite3_column_int(stmt, 0) == 0 ? 1 : 0;
    } else {
        nlog_warn("query `%s` fail: %s", sql, sqlite3_errmsg(global_db));
        rv = -1;
    }

end:
    sqlite3_finalize(stmt);
    return rv;
}

/**
 * 应用架构文件到数据库
 *
 * @param db    数据库连接
 * @param dir   目录路径
 * @param file  文件名
 *
 * @return 成功返回0，失败返回-1
 */
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

/**
 * 收集目录中的架构文件
 *
 * @param dir 目录路径
 *
 * @return 架构文件列表数组，失败返回NULL
 */
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

/**
 * 应用架构文件到数据库
 *
 * @param db   数据库连接
 * @param dir  架构文件目录
 *
 * @return 成功返回0，失败返回错误码
 */
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

/**
 * 加载节点计数
 *
 * @return 成功返回0，失败返回-1
 */
static inline int node_count_load()
{
    int           rv    = 0;
    sqlite3_stmt *stmt  = NULL;
    char *        query = "select count(*) from nodes";

    if (SQLITE_OK == sqlite3_prepare_v2(global_db, query, -1, &stmt, NULL) &&
        SQLITE_ROW == sqlite3_step(stmt)) {
        pthread_rwlock_wrlock(&global_rwlock);
        global_node_count = sqlite3_column_int(stmt, 0);
        pthread_rwlock_unlock(&global_rwlock);
    } else {
        rv = -1;
    }

    sqlite3_finalize(stmt);
    return rv;
}

/**
 * 加载标签计数
 *
 * @return 成功返回0，失败返回-1
 */
static inline int tag_count_load()
{
    int           rv    = 0;
    sqlite3_stmt *stmt  = NULL;
    char *        query = "select count(*) from tags";

    if (SQLITE_OK == sqlite3_prepare_v2(global_db, query, -1, &stmt, NULL) &&
        SQLITE_ROW == sqlite3_step(stmt)) {
        pthread_rwlock_wrlock(&global_rwlock);
        global_tag_count = sqlite3_column_int(stmt, 0);
        pthread_rwlock_unlock(&global_rwlock);
    } else {
        rv = -1;
    }

    sqlite3_finalize(stmt);
    return rv;
}

/**
 * 创建持久化存储
 *
 * @param schema_dir 架构文件目录
 *
 * @return 成功返回0，失败返回-1
 */
int neu_persister_create(const char *schema_dir)
{
    int rv = sqlite3_open(db_file, &global_db);

    if (SQLITE_OK != rv) {
        nlog_fatal("db `%s` fail: %s", db_file, sqlite3_errstr(rv));
        return -1;
    }
    sqlite3_busy_timeout(global_db, 100 * 1000);

    rv = sqlite3_exec(global_db, "PRAGMA foreign_keys=ON", NULL, NULL, NULL);
    if (rv != SQLITE_OK) {
        nlog_fatal("db foreign key support fail: %s",
                   sqlite3_errmsg(global_db));
        sqlite3_close(global_db);
        return -1;
    }

    rv = sqlite3_exec(global_db, "PRAGMA journal_mode=WAL", NULL, NULL, NULL);
    if (rv != SQLITE_OK) {
        nlog_fatal("db journal_mode WAL fail: %s", sqlite3_errmsg(global_db));
        sqlite3_close(global_db);
        return -1;
    }

    rv = apply_schemas(global_db, schema_dir);
    if (rv != 0) {
        nlog_fatal("db apply schemas fail");
        sqlite3_close(global_db);
        return -1;
    }

    if (0 != node_count_load() || 0 != tag_count_load()) {
        nlog_fatal("load node/tag count fail");
        sqlite3_close(global_db);
        return -1;
    }

    return 0;
}

/**
 * 获取数据库连接
 *
 * @return 数据库连接对象
 */
sqlite3 *neu_persister_get_db()
{
    return global_db;
}

/**
 * 增加节点计数
 *
 * @param n 增加的数量
 */
static inline void node_count_add(int n)
{
    pthread_rwlock_wrlock(&global_rwlock);
    global_node_count += n;
    pthread_rwlock_unlock(&global_rwlock);
}

/**
 * 获取节点计数
 *
 * @return 节点数量
 */
int neu_persister_node_count()
{
    pthread_rwlock_rdlock(&global_rwlock);
    int cnt = global_node_count;
    pthread_rwlock_unlock(&global_rwlock);
    return cnt;
}

/**
 * 增加标签计数
 *
 * @param n 增加的数量
 */
static inline void tag_count_add(int n)
{
    pthread_rwlock_wrlock(&global_rwlock);
    global_tag_count += n;
    pthread_rwlock_unlock(&global_rwlock);
}

/**
 * 获取标签计数
 *
 * @return 标签数量
 */
int neu_persister_tag_count()
{
    pthread_rwlock_rdlock(&global_rwlock);
    int cnt = global_tag_count;
    pthread_rwlock_unlock(&global_rwlock);
    return cnt;
}

/**
 * 销毁持久化存储
 */
void neu_persister_destroy()
{
    sqlite3_close(global_db);
}

/**
 * 存储节点信息
 *
 * @param info 节点信息
 *
 * @return 成功返回0，失败返回错误码
 */
int neu_persister_store_node(neu_persist_node_info_t *info)
{
    int rv = 0;
    rv     = execute_sql(global_db,
                     "INSERT INTO nodes (name, type, state, plugin_name) "
                     "VALUES (%Q, %i, %i, %Q)",
                     info->name, info->type, info->state, info->plugin_name);
    if (0 == rv) {
        node_count_add(1);
    }
    return rv;
}

static UT_icd node_info_icd = {
    sizeof(neu_persist_node_info_t),
    NULL,
    NULL,
    (dtor_f *) neu_persist_node_info_fini,
};

/**
 * 加载所有节点信息
 *
 * @param node_infos 输出参数，保存节点信息数组
 *
 * @return 成功返回0，失败返回错误码
 */
int neu_persister_load_nodes(UT_array **node_infos)
{
    int           rv    = 0;
    sqlite3_stmt *stmt  = NULL;
    const char *  query = "SELECT name, type, state, plugin_name FROM nodes;";

    utarray_new(*node_infos, &node_info_icd);

    if (SQLITE_OK != sqlite3_prepare_v2(global_db, query, -1, &stmt, NULL)) {
        nlog_error("prepare `%s` fail: %s", query, sqlite3_errmsg(global_db));
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
        nlog_warn("query `%s` fail: %s", query, sqlite3_errmsg(global_db));
        // do not set return code, return partial or empty result
    }

    sqlite3_finalize(stmt);
    return rv;
}

/**
 * 删除节点
 *
 * @param node_name 节点名称
 *
 * @return 成功返回0，失败返回错误码
 */
int neu_persister_delete_node(const char *node_name)
{
    // rely on foreign key constraints to remove settings, groups, tags and
    // subscriptions
    int rv =
        execute_sql(global_db, "DELETE FROM nodes WHERE name=%Q;", node_name);
    if (0 == rv) {
        node_count_add(-1);
        tag_count_load();
    }
    return rv;
}

/**
 * 更新节点名称
 *
 * @param node_name 原节点名称
 * @param new_name  新节点名称
 *
 * @return 成功返回0，失败返回错误码
 */
int neu_persister_update_node(const char *node_name, const char *new_name)
{
    return execute_sql(global_db, "UPDATE nodes SET name=%Q WHERE name=%Q;",
                       new_name, node_name);
}

/**
 * 更新节点状态
 *
 * @param node_name 节点名称
 * @param state     新状态
 *
 * @return 成功返回0，失败返回错误码
 */
int neu_persister_update_node_state(const char *node_name, int state)
{
    return execute_sql(global_db, "UPDATE nodes SET state=%i WHERE name=%Q;",
                       state, node_name);
}

/**
 * 存储插件信息
 *
 * @param plugin_infos 插件信息数组
 *
 * @return 成功返回0，失败返回错误码
 */
int neu_persister_store_plugins(UT_array *plugin_infos)
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

    rv = write_file_string(plugin_file, result);

    free(result);
    return rv;
}

/**
 * 从文件加载插件信息
 *
 * @param fname       文件名
 * @param plugin_infos 输出参数，保存插件信息数组
 *
 * @return 成功返回0，失败返回错误码
 */
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

/**
 * 字符串比较函数
 *
 * @param a 第一个字符串指针
 * @param b 第二个字符串指针
 *
 * @return 比较结果
 */
static int ut_str_cmp(const void *a, const void *b)
{
    return strcmp(*(char **) a, *(char **) b);
}

/**
 * 加载插件信息
 *
 * @param plugin_infos 输出参数，保存插件信息数组
 *
 * @return 成功返回0，失败返回错误码
 */
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

/**
 * 存储标签
 *
 * @param driver_name 驱动名称
 * @param group_name  组名称
 * @param tag         标签信息
 *
 * @return 成功返回0，失败返回错误码
 */
int neu_persister_store_tag(const char *driver_name, const char *group_name,
                            const neu_datatag_t *tag)
{
    char *val_str = neu_tag_dump_static_value(tag);
    int   rv      = execute_sql(global_db,
                         "INSERT INTO tags ("
                         " driver_name, group_name, name, address, attribute,"
                         " precision, type, decimal, description, value"
                         ") VALUES (%Q, %Q, %Q, %Q, %i, %i, %i, %lf, %Q, %Q)",
                         driver_name, group_name, tag->name, tag->address,
                         tag->attribute, tag->precision, tag->type,
                         tag->decimal, tag->description, val_str);

    if (0 == rv) {
        tag_count_add(1);
    }
    free(val_str);
    return rv;
}

/**
 * 批量添加标签
 *
 * @param query SQL查询语句
 * @param stmt  预处理语句对象
 * @param tags  标签数组
 * @param n     标签数量
 *
 * @return 成功返回0，失败返回-1
 */
static int put_tags(const char *query, sqlite3_stmt *stmt,
                    const neu_datatag_t *tags, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        const neu_datatag_t *tag = &tags[i];

        sqlite3_reset(stmt);

        if (SQLITE_OK != sqlite3_bind_text(stmt, 3, tag->name, -1, NULL)) {
            nlog_error("bind `%s` with name=`%s` fail: %s", query, tag->name,
                       sqlite3_errmsg(global_db));
            return -1;
        }

        if (SQLITE_OK != sqlite3_bind_text(stmt, 4, tag->address, -1, NULL)) {
            nlog_error("bind `%s` with address=`%s` fail: %s", query,
                       tag->address, sqlite3_errmsg(global_db));
            return -1;
        }

        if (SQLITE_OK != sqlite3_bind_int(stmt, 5, tag->attribute)) {
            nlog_error("bind `%s` with attribute=`%i` fail: %s", query,
                       tag->attribute, sqlite3_errmsg(global_db));
            return -1;
        }

        if (SQLITE_OK != sqlite3_bind_int(stmt, 6, tag->precision)) {
            nlog_error("bind `%s` with precision=`%i` fail: %s", query,
                       tag->precision, sqlite3_errmsg(global_db));
            return -1;
        }

        if (SQLITE_OK != sqlite3_bind_int(stmt, 7, tag->type)) {
            nlog_error("bind `%s` with type=`%i` fail: %s", query, tag->type,
                       sqlite3_errmsg(global_db));
            return -1;
        }

        if (SQLITE_OK != sqlite3_bind_double(stmt, 8, tag->decimal)) {
            nlog_error("bind `%s` with decimal=`%f` fail: %s", query,
                       tag->decimal, sqlite3_errmsg(global_db));
            return -1;
        }

        if (SQLITE_OK !=
            sqlite3_bind_text(stmt, 9, tag->description, -1, NULL)) {
            nlog_error("bind `%s` with description=`%s` fail: %s", query,
                       tag->description, sqlite3_errmsg(global_db));
            return -1;
        }

        char *val_str = neu_tag_dump_static_value(tag);
        if (SQLITE_OK != sqlite3_bind_text(stmt, 10, val_str, -1, NULL)) {
            nlog_error("bind `%s` with value=`%s` fail: %s", query, val_str,
                       sqlite3_errmsg(global_db));
            free(val_str);
            return -1;
        }

        if (SQLITE_DONE != sqlite3_step(stmt)) {
            nlog_error("sqlite3_step fail: %s", sqlite3_errmsg(global_db));
            free(val_str);
            return -1;
        }

        free(val_str);
    }

    return 0;
}

/**
 * 批量存储标签
 *
 * @param driver_name 驱动名称
 * @param group_name  组名称
 * @param tags        标签数组
 * @param n           标签数量
 *
 * @return 成功返回0，失败返回错误码
 */
int neu_persister_store_tags(const char *driver_name, const char *group_name,
                             const neu_datatag_t *tags, size_t n)
{
    sqlite3_stmt *stmt  = NULL;
    const char *  query = "INSERT INTO tags ("
                        " driver_name, group_name, name, address, attribute,"
                        " precision, type, decimal, description, value"
                        ") VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10)";

    if (SQLITE_OK != sqlite3_exec(global_db, "BEGIN", NULL, NULL, NULL)) {
        nlog_error("begin transaction fail: %s", sqlite3_errmsg(global_db));
        return NEU_ERR_EINTERNAL;
    }

    if (SQLITE_OK != sqlite3_prepare_v2(global_db, query, -1, &stmt, NULL)) {
        nlog_error("prepare `%s` fail: %s", query, sqlite3_errmsg(global_db));
        goto error;
    }

    if (SQLITE_OK != sqlite3_bind_text(stmt, 1, driver_name, -1, NULL)) {
        nlog_error("bind `%s` with driver_name=`%s` fail: %s", query,
                   driver_name, sqlite3_errmsg(global_db));
        goto error;
    }

    if (SQLITE_OK != sqlite3_bind_text(stmt, 2, group_name, -1, NULL)) {
        nlog_error("bind `%s` with group_name=`%s` fail: %s", query, group_name,
                   sqlite3_errmsg(global_db));
        goto error;
    }

    if (0 != put_tags(query, stmt, tags, n)) {
        goto error;
    }

    if (SQLITE_OK != sqlite3_exec(global_db, "COMMIT", NULL, NULL, NULL)) {
        nlog_error("commit transaction fail: %s", sqlite3_errmsg(global_db));
        goto error;
    }

    tag_count_add(n);
    sqlite3_finalize(stmt);
    return 0;

error:
    nlog_warn("rollback transaction");
    sqlite3_exec(global_db, "ROLLBACK", NULL, NULL, NULL);
    sqlite3_finalize(stmt);
    return NEU_ERR_EINTERNAL;
}

/**
 * 收集标签信息
 *
 * @param stmt 预处理语句对象
 * @param tags 输出参数，保存标签数组
 *
 * @return 成功返回0，失败返回-1
 */
static int collect_tag_info(sqlite3_stmt *stmt, UT_array **tags)
{
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
        if (neu_tag_attribute_test(&tag, NEU_ATTRIBUTE_STATIC)) {
            neu_tag_load_static_value(utarray_back(*tags),
                                      (char *) sqlite3_column_text(stmt, 7));
        }

        step = sqlite3_step(stmt);
    }

    if (SQLITE_DONE != step) {
        return -1;
    }

    return 0;
}

/**
 * 加载标签
 *
 * @param driver_name 驱动名称
 * @param group_name  组名称
 * @param tags        输出参数，保存标签数组
 *
 * @return 成功返回0，失败返回错误码
 */
int neu_persister_load_tags(const char *driver_name, const char *group_name,
                            UT_array **tags)
{
    sqlite3_stmt *stmt  = NULL;
    const char *  query = "SELECT name, address, attribute, precision, type, "
                        "decimal, description, value "
                        "FROM tags WHERE driver_name=? AND group_name=? "
                        "ORDER BY rowid ASC";

    utarray_new(*tags, neu_tag_get_icd());

    if (SQLITE_OK != sqlite3_prepare_v2(global_db, query, -1, &stmt, NULL)) {
        nlog_error("prepare `%s` fail: %s", query, sqlite3_errmsg(global_db));
        goto error;
    }

    if (SQLITE_OK != sqlite3_bind_text(stmt, 1, driver_name, -1, NULL)) {
        nlog_error("bind `%s` with `%s` fail: %s", query, driver_name,
                   sqlite3_errmsg(global_db));
        goto error;
    }

    if (SQLITE_OK != sqlite3_bind_text(stmt, 2, group_name, -1, NULL)) {
        nlog_error("bind `%s` with `%s` fail: %s", query, group_name,
                   sqlite3_errmsg(global_db));
        goto error;
    }

    if (0 != collect_tag_info(stmt, tags)) {
        nlog_warn("query `%s` fail: %s", query, sqlite3_errmsg(global_db));
        // do not set return code, return partial or empty result
    }

    sqlite3_finalize(stmt);
    return 0;

error:
    utarray_free(*tags);
    *tags = NULL;
    return NEU_ERR_EINTERNAL;
}

/**
 * 更新标签
 *
 * @param driver_name 驱动名称
 * @param group_name  组名称
 * @param tag         标签信息
 *
 * @return 成功返回0，失败返回错误码
 */
int neu_persister_update_tag(const char *driver_name, const char *group_name,
                             const neu_datatag_t *tag)
{
    char *val_str = neu_tag_dump_static_value(tag);
    int   rv      = execute_sql(global_db,
                         "UPDATE tags SET"
                         " address=%Q, attribute=%i, precision=%i, type=%i,"
                         " decimal=%lf, description=%Q, value=%Q "
                         "WHERE driver_name=%Q AND group_name=%Q AND name=%Q",
                         tag->address, tag->attribute, tag->precision,
                         tag->type, tag->decimal, tag->description, val_str,
                         driver_name, group_name, tag->name);
    free(val_str);
    return rv;
}

/**
 * 更新标签值
 *
 * @param driver_name 驱动名称
 * @param group_name  组名称
 * @param tag         标签信息
 *
 * @return 成功返回0，失败返回错误码
 */
int neu_persister_update_tag_value(const char *         driver_name,
                                   const char *         group_name,
                                   const neu_datatag_t *tag)
{
    char *val_str = neu_tag_dump_static_value(tag);
    int   rv      = execute_sql(global_db,
                         "UPDATE tags SET value=%Q "
                         "WHERE driver_name=%Q AND group_name=%Q AND name=%Q",
                         val_str, driver_name, group_name, tag->name);
    free(val_str);
    return rv;
}

/**
 * 删除标签
 *
 * @param driver_name 驱动名称
 * @param group_name  组名称
 * @param tag_name    标签名称
 *
 * @return 成功返回0，失败返回错误码
 */
int neu_persister_delete_tag(const char *driver_name, const char *group_name,
                             const char *tag_name)
{
    int rv = execute_sql(
        global_db,
        "DELETE FROM tags WHERE driver_name=%Q AND group_name=%Q AND name=%Q",
        driver_name, group_name, tag_name);
    if (0 == rv) {
        tag_count_add(-1);
    }
    return rv;
}

/**
 * 存储订阅关系
 *
 * @param app_name    应用名称
 * @param driver_name 驱动名称
 * @param group_name  组名称
 * @param params      订阅参数
 *
 * @return 成功返回0，失败返回错误码
 */
int neu_persister_store_subscription(const char *app_name,
                                     const char *driver_name,
                                     const char *group_name, const char *params)
{
    return execute_sql(
        global_db,
        "INSERT INTO subscriptions (app_name, driver_name, group_name, params) "
        "VALUES (%Q, %Q, %Q, %Q)",
        app_name, driver_name, group_name, params);
}

/**
 * 更新订阅参数
 *
 * @param app_name    应用名称
 * @param driver_name 驱动名称
 * @param group_name  组名称
 * @param params      新订阅参数
 *
 * @return 成功返回0，失败返回错误码
 */
int neu_persister_update_subscription(const char *app_name,
                                      const char *driver_name,
                                      const char *group_name,
                                      const char *params)
{
    return execute_sql(global_db,
                       "UPDATE subscriptions SET params=%Q "
                       "WHERE app_name=%Q AND driver_name=%Q AND group_name=%Q",
                       params, app_name, driver_name, group_name);
}

static UT_icd subscription_info_icd = {
    sizeof(neu_persist_subscription_info_t),
    NULL,
    NULL,
    (dtor_f *) neu_persist_subscription_info_fini,
};

/**
 * 加载应用的所有订阅关系
 *
 * @param app_name          应用名称
 * @param subscription_infos 输出参数，保存订阅信息数组
 *
 * @return 成功返回0，失败返回错误码
 */
int neu_persister_load_subscriptions(const char *app_name,
                                     UT_array ** subscription_infos)
{
    sqlite3_stmt *stmt  = NULL;
    const char *  query = "SELECT driver_name, group_name, params "
                        "FROM subscriptions WHERE app_name=?";

    utarray_new(*subscription_infos, &subscription_info_icd);

    if (SQLITE_OK != sqlite3_prepare_v2(global_db, query, -1, &stmt, NULL)) {
        nlog_error("prepare `%s` fail: %s", query, sqlite3_errmsg(global_db));
        goto error;
    }

    if (SQLITE_OK != sqlite3_bind_text(stmt, 1, app_name, -1, NULL)) {
        nlog_error("bind `%s` with `%s` fail: %s", query, app_name,
                   sqlite3_errmsg(global_db));
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

        neu_persist_subscription_info_t info = {
            .driver_name = driver_name,
            .group_name  = group_name,
            .params      = params,
        };
        utarray_push_back(*subscription_infos, &info);

        step = sqlite3_step(stmt);
    }

    if (SQLITE_DONE != step) {
        nlog_warn("query `%s` fail: %s", query, sqlite3_errmsg(global_db));
        // do not set return code, return partial or empty result
    }

    sqlite3_finalize(stmt);
    return 0;

error:
    utarray_free(*subscription_infos);
    *subscription_infos = NULL;
    return NEU_ERR_EINTERNAL;
}

/**
 * 删除订阅关系
 *
 * @param app_name    应用名称
 * @param driver_name 驱动名称
 * @param group_name  组名称
 *
 * @return 成功返回0，失败返回错误码
 */
int neu_persister_delete_subscription(const char *app_name,
                                      const char *driver_name,
                                      const char *group_name)
{
    return execute_sql(global_db,
                       "DELETE FROM subscriptions WHERE app_name=%Q AND "
                       "driver_name=%Q AND group_name=%Q",
                       app_name, driver_name, group_name);
}

/**
 * 存储组信息
 *
 * @param driver_name 驱动名称
 * @param group_info  组信息
 *
 * @return 成功返回0，失败返回错误码
 */
int neu_persister_store_group(const char *              driver_name,
                              neu_persist_group_info_t *group_info)
{
    return execute_sql(
        global_db,
        "INSERT INTO groups (driver_name, name, interval) VALUES (%Q, %Q, %u)",
        driver_name, group_info->name, (unsigned) group_info->interval);
}

/**
 * 更新组信息
 *
 * @param driver_name 驱动名称
 * @param group_name  原组名称
 * @param group_info  新组信息
 *
 * @return 成功返回0，失败返回错误码
 */
int neu_persister_update_group(const char *driver_name, const char *group_name,
                               neu_persist_group_info_t *group_info)
{
    int  ret             = -1;
    bool update_name     = (0 != strcmp(group_name, group_info->name));
    bool update_interval = (NEU_GROUP_INTERVAL_LIMIT <= group_info->interval);

    if (update_name && update_interval) {
        ret = execute_sql(global_db,
                          "UPDATE groups SET name=%Q, interval=%i "
                          "WHERE driver_name=%Q AND name=%Q",
                          group_info->name, group_info->interval, driver_name,
                          group_name);
    } else if (update_name) {
        ret = execute_sql(global_db,
                          "UPDATE groups SET name=%Q "
                          "WHERE driver_name=%Q AND name=%Q",
                          group_info->name, driver_name, group_name);
    } else if (update_interval) {
        ret = execute_sql(global_db,
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

/**
 * 收集组信息
 *
 * @param stmt        预处理语句对象
 * @param group_infos 输出参数，保存组信息数组
 *
 * @return 成功返回0，失败返回-1
 */
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
        utarray_push_back(*group_infos, &info);

        step = sqlite3_step(stmt);
    }

    if (SQLITE_DONE != step) {
        return -1;
    }

    return 0;
}

/**
 * 加载驱动的所有组信息
 *
 * @param driver_name 驱动名称
 * @param group_infos 输出参数，保存组信息数组
 *
 * @return 成功返回0，失败返回错误码
 */
int neu_persister_load_groups(const char *driver_name, UT_array **group_infos)
{
    sqlite3_stmt *stmt = NULL;
    const char *query = "SELECT name, interval FROM groups WHERE driver_name=?";

    utarray_new(*group_infos, &group_info_icd);

    if (SQLITE_OK != sqlite3_prepare_v2(global_db, query, -1, &stmt, NULL)) {
        nlog_error("prepare `%s` fail: %s", query, sqlite3_errmsg(global_db));
        goto error;
    }

    if (SQLITE_OK != sqlite3_bind_text(stmt, 1, driver_name, -1, NULL)) {
        nlog_error("bind `%s` with `%s` fail: %s", query, driver_name,
                   sqlite3_errmsg(global_db));
        goto error;
    }

    if (0 != collect_group_info(stmt, group_infos)) {
        nlog_warn("query `%s` fail: %s", query, sqlite3_errmsg(global_db));
        // do not set return code, return partial or empty result
    }

    sqlite3_finalize(stmt);
    return 0;

error:
    utarray_free(*group_infos);
    *group_infos = NULL;
    return NEU_ERR_EINTERNAL;
}

/**
 * 删除组
 *
 * @param driver_name 驱动名称
 * @param group_name  组名称
 *
 * @return 成功返回0，失败返回错误码
 */
int neu_persister_delete_group(const char *driver_name, const char *group_name)
{
    // rely on foreign key constraints to delete tags and subscriptions
    int rv = execute_sql(global_db,
                         "DELETE FROM groups WHERE driver_name=%Q AND name=%Q",
                         driver_name, group_name);
    if (0 == rv) {
        tag_count_load();
    }
    return rv;
}

/**
 * 存储节点设置
 *
 * @param node_name 节点名称
 * @param setting   设置内容
 *
 * @return 成功返回0，失败返回错误码
 */
int neu_persister_store_node_setting(const char *node_name, const char *setting)
{
    return execute_sql(
        global_db,
        "INSERT OR REPLACE INTO settings (node_name, setting) VALUES (%Q, %Q)",
        node_name, setting);
}

/**
 * 加载节点设置
 *
 * @param node_name 节点名称
 * @param setting   输出参数，保存设置内容
 *
 * @return 成功返回0，失败返回错误码
 */
int neu_persister_load_node_setting(const char *       node_name,
                                    const char **const setting)
{
    int           rv    = 0;
    sqlite3_stmt *stmt  = NULL;
    const char *  query = "SELECT setting FROM settings WHERE node_name=?";

    if (SQLITE_OK != sqlite3_prepare_v2(global_db, query, -1, &stmt, NULL)) {
        nlog_error("prepare `%s` with `%s` fail: %s", query, node_name,
                   sqlite3_errmsg(global_db));
        return NEU_ERR_EINTERNAL;
    }

    if (SQLITE_OK != sqlite3_bind_text(stmt, 1, node_name, -1, NULL)) {
        nlog_error("bind `%s` with `%s` fail: %s", query, node_name,
                   sqlite3_errmsg(global_db));
        rv = NEU_ERR_EINTERNAL;
        goto end;
    }

    if (SQLITE_ROW != sqlite3_step(stmt)) {
        nlog_warn("SQL `%s` with `%s` fail: %s", query, node_name,
                  sqlite3_errmsg(global_db));
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

/**
 * 删除节点设置
 *
 * @param node_name 节点名称
 *
 * @return 成功返回0，失败返回错误码
 */
int neu_persister_delete_node_setting(const char *node_name)
{
    return execute_sql(global_db, "DELETE FROM settings WHERE node_name=%Q",
                       node_name);
}

/**
 * 存储用户信息
 *
 * @param user 用户信息
 *
 * @return 成功返回0，失败返回错误码
 */
int neu_persister_store_user(const neu_persist_user_info_t *user)
{
    return execute_sql(global_db,
                       "INSERT INTO users (name, password) VALUES (%Q, %Q)",
                       user->name, user->hash);
}

/**
 * 更新用户信息
 *
 * @param user 用户信息
 *
 * @return 成功返回0，失败返回错误码
 */
int neu_persister_update_user(const neu_persist_user_info_t *user)
{
    return execute_sql(global_db, "UPDATE users SET password=%Q WHERE name=%Q",
                       user->hash, user->name);
}

/**
 * 加载用户信息
 *
 * @param user_name 用户名
 * @param user_p    输出参数，保存用户信息
 *
 * @return 成功返回0，失败返回错误码
 */
int neu_persister_load_user(const char *              user_name,
                            neu_persist_user_info_t **user_p)
{
    neu_persist_user_info_t *user  = NULL;
    sqlite3_stmt *           stmt  = NULL;
    const char *             query = "SELECT password FROM users WHERE name=?";

    if (SQLITE_OK != sqlite3_prepare_v2(global_db, query, -1, &stmt, NULL)) {
        nlog_error("prepare `%s` with `%s` fail: %s", query, user_name,
                   sqlite3_errmsg(global_db));
        return NEU_ERR_EINTERNAL;
    }

    if (SQLITE_OK != sqlite3_bind_text(stmt, 1, user_name, -1, NULL)) {
        nlog_error("bind `%s` with `%s` fail: %s", query, user_name,
                   sqlite3_errmsg(global_db));
        goto error;
    }

    if (SQLITE_ROW != sqlite3_step(stmt)) {
        nlog_warn("SQL `%s` with `%s` fail: %s", query, user_name,
                  sqlite3_errmsg(global_db));
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

/**
 * 删除用户
 *
 * @param user_name 用户名
 *
 * @return 成功返回0，失败返回错误码
 */
int neu_persister_delete_user(const char *user_name)
{
    return execute_sql(global_db, "DELETE FROM users WHERE name=%Q", user_name);
}

/**
 * 存储模板信息
 *
 * @param name   模板名称
 * @param plugin 插件名称
 *
 * @return 成功返回0，失败返回错误码
 */
int neu_persister_store_template(const char *name, const char *plugin)
{
    return execute_sql(global_db,
                       "INSERT INTO templates (name, plugin_name) "
                       "VALUES (%Q, %Q)",
                       name, plugin);
}

/**
 * 释放模板信息
 *
 * @param info 模板信息
 */
static inline void
neu_persist_template_info_fini(neu_persist_template_info_t *info)
{
    free(info->name);
    free(info->plugin_name);
}

static UT_icd template_info_icd = {
    sizeof(neu_persist_template_info_t),
    NULL,
    NULL,
    (dtor_f *) neu_persist_template_info_fini,
};

/**
 * 加载所有模板信息
 *
 * @param infos 输出参数，保存模板信息数组
 *
 * @return 成功返回0，失败返回错误码
 */
int neu_persister_load_templates(UT_array **infos)
{
    int           rv    = 0;
    sqlite3_stmt *stmt  = NULL;
    const char *  query = "SELECT name, plugin_name FROM templates;";

    if (SQLITE_OK != sqlite3_prepare_v2(global_db, query, -1, &stmt, NULL)) {
        nlog_error("prepare `%s` fail: %s", query, sqlite3_errmsg(global_db));
        return NEU_ERR_EINTERNAL;
    }

    utarray_new(*infos, &template_info_icd);

    int step = sqlite3_step(stmt);
    while (SQLITE_ROW == step) {
        neu_persist_template_info_t info = {};
        char *name = strdup((char *) sqlite3_column_text(stmt, 0));
        if (NULL == name) {
            break;
        }

        char *plugin_name = strdup((char *) sqlite3_column_text(stmt, 1));
        if (NULL == plugin_name) {
            free(name);
            break;
        }

        info.name        = name;
        info.plugin_name = plugin_name;
        utarray_push_back(*infos, &info);

        step = sqlite3_step(stmt);
    }

    if (SQLITE_DONE != step) {
        nlog_warn("query `%s` fail: %s", query, sqlite3_errmsg(global_db));
        // do not set return code, return partial or empty result
    }

    sqlite3_finalize(stmt);
    return rv;
}

/**
 * 删除模板
 *
 * @param name 模板名称
 *
 * @return 成功返回0，失败返回错误码
 */
int neu_persister_delete_template(const char *name)
{
    // rely on foreign key constraints to remove groups and tags
    return execute_sql(global_db, "DELETE FROM templates WHERE name=%Q;", name);
}

/**
 * 清除所有模板
 *
 * @return 成功返回0，失败返回错误码
 */
int neu_persister_clear_templates()
{
    // rely on foreign key constraints to remove groups and tags
    return execute_sql(global_db, "DELETE FROM templates");
}

/**
 * 存储模板组信息
 *
 * @param tmpl_name  模板名称
 * @param group_info 组信息
 *
 * @return 成功返回0，失败返回错误码
 */
int neu_persister_store_template_group(const char *              tmpl_name,
                                       neu_persist_group_info_t *group_info)
{
    return execute_sql(global_db,
                       "INSERT INTO template_groups ("
                       " tmpl_name, name, interval) VALUES (%Q, %Q, %u)",
                       tmpl_name, group_info->name,
                       (unsigned) group_info->interval);
}

/**
 * 更新模板组信息
 *
 * @param tmpl_name  模板名称
 * @param group_name 原组名称
 * @param group_info 新组信息
 *
 * @return 成功返回0，失败返回错误码
 */
int neu_persister_update_template_group(const char *              tmpl_name,
                                        const char *              group_name,
                                        neu_persist_group_info_t *group_info)
{
    int  ret             = -1;
    bool update_name     = (0 != strcmp(group_name, group_info->name));
    bool update_interval = (NEU_GROUP_INTERVAL_LIMIT <= group_info->interval);

    if (update_name && update_interval) {
        ret = execute_sql(global_db,
                          "UPDATE template_groups SET name=%Q, interval=%i "
                          "WHERE tmpl_name=%Q AND name=%Q",
                          group_info->name, group_info->interval, tmpl_name,
                          group_name);
    } else if (update_name) {
        ret = execute_sql(global_db,
                          "UPDATE template_groups SET name=%Q "
                          "WHERE tmpl_name=%Q AND name=%Q",
                          group_info->name, tmpl_name, group_name);
    } else if (update_interval) {
        ret = execute_sql(global_db,
                          "UPDATE template_groups SET interval=%i "
                          "WHERE tmpl_name=%Q AND name=%Q",
                          group_info->interval, tmpl_name, group_name);
    }

    return ret;
}

/**
 * 加载模板的所有组信息
 *
 * @param tmpl_name   模板名称
 * @param group_infos 输出参数，保存组信息数组
 *
 * @return 成功返回0，失败返回错误码
 */
int neu_persister_load_template_groups(const char *tmpl_name,
                                       UT_array ** group_infos)
{
    sqlite3_stmt *stmt = NULL;
    const char *  query =
        "SELECT name, interval FROM template_groups WHERE tmpl_name=?";

    utarray_new(*group_infos, &group_info_icd);

    if (SQLITE_OK != sqlite3_prepare_v2(global_db, query, -1, &stmt, NULL)) {
        nlog_error("prepare `%s` fail: %s", query, sqlite3_errmsg(global_db));
        goto error;
    }

    if (SQLITE_OK != sqlite3_bind_text(stmt, 1, tmpl_name, -1, NULL)) {
        nlog_error("bind `%s` with `%s` fail: %s", query, tmpl_name,
                   sqlite3_errmsg(global_db));
        goto error;
    }

    if (0 != collect_group_info(stmt, group_infos)) {
        nlog_warn("query `%s` fail: %s", query, sqlite3_errmsg(global_db));
        // do not set return code, return partial or empty result
    }

    sqlite3_finalize(stmt);
    return 0;

error:
    utarray_free(*group_infos);
    *group_infos = NULL;
    return NEU_ERR_EINTERNAL;
}

/**
 * 删除模板组
 *
 * @param tmpl_name  模板名称
 * @param group_name 组名称
 *
 * @return 成功返回0，失败返回错误码
 */
int neu_persister_delete_template_group(const char *tmpl_name,
                                        const char *group_name)
{
    // rely on foreign key constraints to delete tags
    return execute_sql(
        global_db, "DELETE FROM template_groups WHERE tmpl_name=%Q AND name=%Q",
        tmpl_name, group_name);
}

/**
 * 存储模板标签
 *
 * @param tmpl_name  模板名称
 * @param group_name 组名称
 * @param tags       标签数组
 * @param n          标签数量
 *
 * @return 成功返回0，失败返回错误码
 */
int neu_persister_store_template_tags(const char *         tmpl_name,
                                      const char *         group_name,
                                      const neu_datatag_t *tags, size_t n)
{
    char *        val_str = NULL;
    sqlite3_stmt *stmt    = NULL;
    const char *  query   = "INSERT INTO template_tags ("
                        " tmpl_name, group_name, name, address, attribute,"
                        " precision, type, decimal, description, value"
                        ") VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10)";

    if (SQLITE_OK != sqlite3_exec(global_db, "BEGIN", NULL, NULL, NULL)) {
        nlog_error("begin transaction fail: %s", sqlite3_errmsg(global_db));
        return NEU_ERR_EINTERNAL;
    }

    if (SQLITE_OK != sqlite3_prepare_v2(global_db, query, -1, &stmt, NULL)) {
        nlog_error("prepare `%s` fail: %s", query, sqlite3_errmsg(global_db));
        goto error;
    }

    if (SQLITE_OK != sqlite3_bind_text(stmt, 1, tmpl_name, -1, NULL)) {
        nlog_error("bind `%s` with tmpl_name=`%s` fail: %s", query, tmpl_name,
                   sqlite3_errmsg(global_db));
        goto error;
    }

    if (SQLITE_OK != sqlite3_bind_text(stmt, 2, group_name, -1, NULL)) {
        nlog_error("bind `%s` with group_name=`%s` fail: %s", query, group_name,
                   sqlite3_errmsg(global_db));
        goto error;
    }

    if (0 != put_tags(query, stmt, tags, n)) {
        goto error;
    }

    if (SQLITE_OK != sqlite3_exec(global_db, "COMMIT", NULL, NULL, NULL)) {
        nlog_error("commit transaction fail: %s", sqlite3_errmsg(global_db));
        goto error;
    }

    sqlite3_finalize(stmt);
    return 0;

error:
    nlog_info("rollback transaction");
    sqlite3_exec(global_db, "ROLLBACK", NULL, NULL, NULL);
    sqlite3_finalize(stmt);
    free(val_str);
    return NEU_ERR_EINTERNAL;
}

/**
 * 加载模板标签
 *
 * @param tmpl_name  模板名称
 * @param group_name 组名称
 * @param tags       输出参数，保存标签数组
 *
 * @return 成功返回0，失败返回错误码
 */
int neu_persister_load_template_tags(const char *tmpl_name,
                                     const char *group_name, UT_array **tags)
{
    sqlite3_stmt *stmt  = NULL;
    const char *  query = "SELECT name, address, attribute, precision, type, "
                        "decimal, description, value "
                        "FROM template_tags WHERE tmpl_name=? AND group_name=? "
                        "ORDER BY rowid ASC";

    utarray_new(*tags, neu_tag_get_icd());

    if (SQLITE_OK != sqlite3_prepare_v2(global_db, query, -1, &stmt, NULL)) {
        nlog_error("prepare `%s` fail: %s", query, sqlite3_errmsg(global_db));
        goto error;
    }

    if (SQLITE_OK != sqlite3_bind_text(stmt, 1, tmpl_name, -1, NULL)) {
        nlog_error("bind `%s` with `%s` fail: %s", query, tmpl_name,
                   sqlite3_errmsg(global_db));
        goto error;
    }

    if (SQLITE_OK != sqlite3_bind_text(stmt, 2, group_name, -1, NULL)) {
        nlog_error("bind `%s` with `%s` fail: %s", query, group_name,
                   sqlite3_errmsg(global_db));
        goto error;
    }

    if (0 != collect_tag_info(stmt, tags)) {
        nlog_warn("query `%s` fail: %s", query, sqlite3_errmsg(global_db));
        // do not set return code, return partial or empty result
    }

    sqlite3_finalize(stmt);
    return 0;

error:
    utarray_free(*tags);
    *tags = NULL;
    return NEU_ERR_EINTERNAL;
}

/**
 * 更新模板标签
 *
 * @param tmpl_name  模板名称
 * @param group_name 组名称
 * @param tags       标签数组
 * @param n          标签数量
 *
 * @return 成功返回0，失败返回错误码
 */
int neu_persister_update_template_tags(const char *         tmpl_name,
                                       const char *         group_name,
                                       const neu_datatag_t *tags, size_t n)
{
    sqlite3_stmt *stmt  = NULL;
    const char *  query = "UPDATE template_tags SET"
                        "  address=?4, attribute=?5, precision=?6, type=?7,"
                        "  decimal=?8, description=?9, value=?10 "
                        "WHERE tmpl_name=?1 AND group_name=?2 AND name=?3";

    if (SQLITE_OK != sqlite3_exec(global_db, "BEGIN", NULL, NULL, NULL)) {
        nlog_error("begin transaction fail: %s", sqlite3_errmsg(global_db));
        return NEU_ERR_EINTERNAL;
    }

    if (SQLITE_OK != sqlite3_prepare_v2(global_db, query, -1, &stmt, NULL)) {
        nlog_error("prepare `%s` fail: %s", query, sqlite3_errmsg(global_db));
        goto error;
    }

    if (SQLITE_OK != sqlite3_bind_text(stmt, 1, tmpl_name, -1, NULL)) {
        nlog_error("bind `%s` with tmpl_name=`%s` fail: %s", query, tmpl_name,
                   sqlite3_errmsg(global_db));
        goto error;
    }

    if (SQLITE_OK != sqlite3_bind_text(stmt, 2, group_name, -1, NULL)) {
        nlog_error("bind `%s` with group_name=`%s` fail: %s", query, group_name,
                   sqlite3_errmsg(global_db));
        goto error;
    }

    if (0 != put_tags(query, stmt, tags, n)) {
        goto error;
    }

    if (SQLITE_OK != sqlite3_exec(global_db, "COMMIT", NULL, NULL, NULL)) {
        nlog_error("commit transaction fail: %s", sqlite3_errmsg(global_db));
        goto error;
    }

    sqlite3_finalize(stmt);
    return 0;

error:
    nlog_info("rollback transaction");
    sqlite3_exec(global_db, "ROLLBACK", NULL, NULL, NULL);
    sqlite3_finalize(stmt);
    return NEU_ERR_EINTERNAL;
}

/**
 * 删除模板标签
 *
 * @param tmpl_name  模板名称
 * @param group_name 组名称
 * @param tags       标签名称数组
 * @param n_tag      标签数量
 *
 * @return 成功返回0，失败返回错误码
 */
int neu_persister_delete_template_tags(const char *       tmpl_name,
                                       const char *       group_name,
                                       const char *const *tags, size_t n_tag)
{
    sqlite3_stmt *stmt  = NULL;
    const char *  query = "DELETE FROM template_tags WHERE tmpl_name=? AND "
                        "group_name=? AND name=?";

    if (SQLITE_OK != sqlite3_exec(global_db, "BEGIN", NULL, NULL, NULL)) {
        nlog_error("begin transaction fail: %s", sqlite3_errmsg(global_db));
        return NEU_ERR_EINTERNAL;
    }

    if (SQLITE_OK != sqlite3_prepare_v2(global_db, query, -1, &stmt, NULL)) {
        nlog_error("prepare `%s` fail: %s", query, sqlite3_errmsg(global_db));
        goto error;
    }

    if (SQLITE_OK != sqlite3_bind_text(stmt, 1, tmpl_name, -1, NULL)) {
        nlog_error("bind `%s` with tmpl_name=`%s` fail: %s", query, tmpl_name,
                   sqlite3_errmsg(global_db));
        goto error;
    }

    if (SQLITE_OK != sqlite3_bind_text(stmt, 2, group_name, -1, NULL)) {
        nlog_error("bind `%s` with group_name=`%s` fail: %s", query, group_name,
                   sqlite3_errmsg(global_db));
        goto error;
    }

    for (size_t i = 0; i < n_tag; ++i) {
        sqlite3_reset(stmt);

        if (SQLITE_OK != sqlite3_bind_text(stmt, 3, tags[i], -1, NULL)) {
            nlog_error("bind `%s` with tmpl_name=`%s` fail: %s", query,
                       tmpl_name, sqlite3_errmsg(global_db));
            goto error;
        }

        if (SQLITE_DONE != sqlite3_step(stmt)) {
            nlog_error("sqlite3_step fail: %s", sqlite3_errmsg(global_db));
            goto error;
        }
    }

    if (SQLITE_OK != sqlite3_exec(global_db, "COMMIT", NULL, NULL, NULL)) {
        nlog_error("commit transaction fail: %s", sqlite3_errmsg(global_db));
        goto error;
    }

    sqlite3_finalize(stmt);
    return 0;

error:
    nlog_info("rollback transaction");
    sqlite3_exec(global_db, "ROLLBACK", NULL, NULL, NULL);
    sqlite3_finalize(stmt);
    return NEU_ERR_EINTERNAL;
}
