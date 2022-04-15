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

#define _XOPEN_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <jwt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "json/neu_json_fn.h"
#include "json/neu_json_log.h"

#include "config.h"
#include "handle.h"
#include "http.h"
#include "neu_vector.h"
#include "utils/neu_jwt.h"

#include "log_handle.h"

#ifdef __GNUC__
#define thread_local __thread
#elif __STDC_VERSION__ >= 201112L
#define thread_local _Thread_local
#elif defined(_MSC_VER)
#define thread_local __declspec(thread)
#else
#error Cannot define thread_local
#endif

#define LOG_TIME_FMT "%Y-%m-%d %H:%M:%S"
#define LOG_FILE "./logs/neuron.log"
#define LOG_PAGE_SIZE_MIN 200
#define LOG_PAGE_SIZE_MAX 10000

static size_t binary_search(FILE *fp, size_t lo, size_t hi, uint64_t ts);
static int    get_log_level(const char *line);
static int get_log(const char *fname, uint64_t since, uint64_t until, int level,
                   uint32_t page, uint32_t page_size, vector_t *lines,
                   size_t *line_tot);
static int string_to_log_level(const char *s, size_t n, int *level);

void handle_get_log(nng_aio *aio)
{
    neu_json_get_log_resp_t resp      = {};
    uint64_t                since     = 0;
    uint64_t                until     = 0;
    const char *            level_str = NULL;
    size_t                  len       = 0;
    int                     level     = NEU_LOG_TRACE;
    uint32_t                page      = 0;
    uint32_t                page_size = 0;
    size_t                  line_tot  = 0;

    VALIDATE_JWT(aio);

    // required params
    if (0 != http_get_param_uint64(aio, "since", &since) ||
        0 != http_get_param_uint64(aio, "until", &until) || since > until ||
        0 != http_get_param_uint32(aio, "page", &page) ||
        0 != http_get_param_uint32(aio, "page_size", &page_size) ||
        page_size < LOG_PAGE_SIZE_MIN || LOG_PAGE_SIZE_MAX < page_size) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_PARAM_IS_WRONG, {
            http_response(aio, error_code.error, result_error);
        });
        return;
    }

    // optional param
    level_str = http_get_param(aio, "level", &len);
    if (NULL != level_str && 0 != string_to_log_level(level_str, len, &level)) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_PARAM_IS_WRONG, {
            http_response(aio, error_code.error, result_error);
        });
        return;
    }

    vector_t lines;
    vector_init(&lines, 1, sizeof(char *));
    if (0 !=
        get_log(LOG_FILE, since, until, level, page, page_size, &lines,
                &line_tot)) {
        resp.error = NEU_ERR_EINTERNAL;
    }
    resp.n_row      = lines.size;
    resp.rows       = lines.data;
    resp.page_count = (line_tot + page_size - 1) / page_size;

    char *result = NULL;
    neu_json_encode_by_fn(&resp, neu_json_encode_get_log_resp, &result);
    http_ok(aio, result);
    VECTOR_FOR_EACH(&lines, iter)
    {
        char *line = *(char **) iterator_get(&iter);
        free(line);
    }
    vector_uninit(&lines);
    free(result);
}

static thread_local struct {
    int      level; // This three indicates a same get log query,
    uint64_t since; // without taking page and page size into account.
    uint64_t until; //

    time_t time;         // time stamp in last query result line
    size_t line_start;   // last query result start line number
    size_t line_end;     // last query result end line number
    size_t line_tot;     // total number of result log lines
    size_t offset_start; // file offset corresponding to line_start
    size_t offset_end;   // file offset corresponding to line_end
} g_log_cache_;

static inline bool validate_cache(FILE *fp, int level, uint64_t since,
                                  uint64_t until)
{
    char      buf[20] = { 0 };
    struct tm tm      = { .tm_isdst = -1 };
    time_t    ts      = 0;

    if (since == g_log_cache_.since && until == g_log_cache_.until &&
        level == g_log_cache_.level) {

        // check against file modification, maybe due to log rotation
        fseek(fp, g_log_cache_.offset_end, SEEK_SET);
        char *ret = fgets(buf, sizeof(buf), fp);
        if (NULL == ret) {
            return false;
        }

        if (NULL != strptime(buf, LOG_TIME_FMT, &tm)) {
            ts = mktime(&tm);
            if (ts == g_log_cache_.time) {
                return true;
            }
        }

        // clear cache
        g_log_cache_.since        = 1;
        g_log_cache_.until        = 0;
        g_log_cache_.time         = 0;
        g_log_cache_.line_start   = 0;
        g_log_cache_.line_end     = 0;
        g_log_cache_.offset_start = 0;
        g_log_cache_.offset_end   = 0;
    }

    return false;
}

// Find log lines within time range.
static int get_log(const char *fname, uint64_t since, uint64_t until, int level,
                   uint32_t page, uint32_t page_size, vector_t *lines,
                   size_t *line_tot)
{
    FILE *fp = fopen(fname, "r");

    if (NULL == fp) {
        return NEU_ERR_ENOFILES;
    }

    int       rv      = 0;
    size_t    i       = 0;
    size_t    size    = 0;
    size_t    n       = 1024;
    struct tm tm      = { .tm_isdst = -1 };
    char *    lineptr = calloc(n, sizeof(char));
    size_t    nline   = 0;
    size_t    start   = page * page_size;
    size_t    end     = start + page_size;

    if (NULL == lineptr) {
        return NEU_ERR_ENOMEM;
    }

    // get file size
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    bool cached = validate_cache(fp, level, since, until);
    if (cached && g_log_cache_.line_start <= start &&
        start < g_log_cache_.line_end + LOG_PAGE_SIZE_MAX) {
        // continue from last file offset
        if (g_log_cache_.line_end <= start) {
            i     = g_log_cache_.offset_end;
            nline = g_log_cache_.line_end;
        } else {
            i     = g_log_cache_.offset_start;
            nline = g_log_cache_.line_start;
        }
    } else {
        i = binary_search(fp, 0, size, since);
    }

    fseek(fp, i, SEEK_SET);

    while (i < size) {
        int len = getline(&lineptr, &n, fp);
        if (len <= 0) {
            break;
        }
        i += len;

        if (get_log_level(lineptr) < level) {
            continue;
        }

        if (NULL == strptime(lineptr, LOG_TIME_FMT, &tm)) {
            continue;
        }
        uint64_t ts = mktime(&tm);
        if (ts < since) {
            continue;
        }
        if (ts >= until) {
            break;
        }
        if (nline < start) {
            ++nline;
            continue;
        } else if (nline == start) {
            g_log_cache_.line_start   = nline;
            g_log_cache_.offset_start = i - len;
        }

        if (nline < end) {
            char *line = strdup(lineptr);
            if (NULL == line) {
                rv = NEU_ERR_ENOMEM;
                break;
            }
            if (0 != vector_push_back(lines, &line)) {
                rv = NEU_ERR_ENOMEM;
                break;
            }

            g_log_cache_.line_end   = nline;
            g_log_cache_.offset_end = i - len;
            g_log_cache_.time       = ts;
        }

        ++nline;

        if (nline == end) {
            if (cached && nline < g_log_cache_.line_tot) {
                // no need to count total number of lines.
                break;
            } else {
                cached = false;
            }
        }
    }

    if (0 != rv || !cached) {
        g_log_cache_.level    = level;
        g_log_cache_.since    = since;
        g_log_cache_.until    = until;
        g_log_cache_.line_tot = nline;
    }

    if (NULL != line_tot) {
        *line_tot = g_log_cache_.line_tot;
    }

    free(lineptr);
    fclose(fp);
    return rv;
}

// Look for file offset, starting at which logs with target time stamp follows.
// The returned offset is not a strict lower bound, just enough for filtering.
static size_t binary_search(FILE *fp, size_t lo, size_t hi, uint64_t ts)
{
    size_t    n       = 1024;
    char *    lineptr = calloc(n, sizeof(char));
    struct tm tm      = { .tm_isdst = -1 };

    while (lo < hi) {
        int    len1 = 0, len2 = 0;
        size_t mid = (lo + hi) / 2;

        fseek(fp, mid, SEEK_SET);

        // scan to a log line following mid
        len1 = getline(&lineptr, &n, fp);
        if (len1 < 0 || mid + len1 >= hi) {
            // outside offset range
            break;
        }
        while (NULL == strptime(lineptr, LOG_TIME_FMT, &tm)) {
            len2 = getline(&lineptr, &n, fp);
            if (len2 < 0 || mid + len1 + len2 >= hi) {
                // outside offset range
                break;
            }
            len1 += len2;
        }
        if (len2 < 0) {
            // outside offset range
            break;
        }

        uint64_t t = mktime(&tm);
        if (ts <= t) {
            hi = mid + len1;
        } else {
            lo = mid + len1;
        }
    }

    free(lineptr);
    return lo;
}

static int get_log_level(const char *line)
{
    int level = NEU_LOG_TRACE;

    line += 19; // skip time string
    while (*line && 'T' != *line && 'D' != *line && 'I' != *line &&
           'W' != *line && 'E' != *line && 'F' != *line) {
        ++line;
    }
    if ('T' == *line && 0 == strncmp(line, "TRACE", 5)) {
        level = NEU_LOG_TRACE;
    } else if ('D' == *line && 0 == strncmp(line, "DEBUG", 5)) {
        level = NEU_LOG_DEBUG;
    } else if ('I' == *line && 0 == strncmp(line, "INFO", 4)) {
        level = NEU_LOG_INFO;
    } else if ('W' == *line && 0 == strncmp(line, "WARN", 4)) {
        level = NEU_LOG_WARN;
    } else if ('E' == *line && 0 == strncmp(line, "ERROR", 5)) {
        level = NEU_LOG_ERROR;
    } else if ('F' == *line && 0 == strncmp(line, "FATAL", 5)) {
        level = NEU_LOG_FATAL;
    }

    return level;
}

static int string_to_log_level(const char *s, size_t n, int *level)
{
    if (NULL == level) {
        return -1;
    }
    if (0 == strncasecmp(s, "trace", n)) {
        *level = NEU_LOG_TRACE;
    } else if (0 == strncasecmp(s, "debug", n)) {
        *level = NEU_LOG_DEBUG;
    } else if (0 == strncasecmp(s, "info", n)) {
        *level = NEU_LOG_INFO;
    } else if (0 == strncasecmp(s, "warn", n)) {
        *level = NEU_LOG_WARN;
    } else if (0 == strncasecmp(s, "error", n)) {
        *level = NEU_LOG_ERROR;
    } else if (0 == strncasecmp(s, "fatal", n)) {
        *level = NEU_LOG_FATAL;
    } else {
        return -1;
    }

    return 0;
}
