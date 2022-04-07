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

#include "log.h"
#include <syslog.h>

#define MAX_CALLBACKS 32
// #define LOG_USE_COLOR 1

typedef struct {
    log_LogFn fn;
    void *    udata;
    int       level;
} Callback;

static struct {
    void *     udata;
    log_LockFn lock;
    int        level;
    bool       quiet;
    Callback   callbacks[MAX_CALLBACKS];
} L;

static const char *level_strings[] = { "TRACE", "DEBUG", "INFO",
                                       "WARN",  "ERROR", "FATAL" };

#ifdef LOG_USE_COLOR
static const char *level_colors[] = { "\x1b[94m", "\x1b[36m", "\x1b[32m",
                                      "\x1b[33m", "\x1b[31m", "\x1b[35m" };
#endif

static void stdout_callback(log_Event *ev)
{
    char buf[64];
    buf[strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", ev->time)] = '\0';
#ifdef LOG_USE_COLOR
    fprintf(ev->udata,
            "%s [%i] %s%-5s\x1b[0m [%s] \x1b[90m%s:%d \x1b[0m %s: ", buf,
            getpid(), level_colors[ev->level], level_strings[ev->level],
            ev->label, ev->file, ev->line, ev->func);
#else
    fprintf(ev->udata, "%s [%i] %-5s [%s] %s:%d %s: ", buf, getpid(),
            level_strings[ev->level], ev->label, ev->file, ev->line, ev->func);
#endif
    vfprintf(ev->udata, ev->fmt, ev->ap);
    fprintf(ev->udata, "\n");
    fflush(ev->udata);
}

static void syslog_callback(log_Event *ev)
{
    int level = LOG_ERR;
    switch (ev->level) {
    case NEU_LOG_TRACE:
        // fall through
    case NEU_LOG_DEBUG:
        level = LOG_DEBUG;
        break;
    case NEU_LOG_INFO:
        level = LOG_INFO;
        break;
    case NEU_LOG_WARN:
        level = LOG_WARNING;
        break;
    case NEU_LOG_ERROR:
        level = LOG_ERR;
        break;
    case NEU_LOG_FATAL:
        level = LOG_ALERT;
        break;
    }
    char   buf[1024] = { 0 };
    size_t n         = snprintf(buf, sizeof(buf), "[%s] %s:%d %s: ", ev->label,
                        ev->file, ev->line, ev->func);
    if (n < sizeof(buf)) {
        // may truncate, but don't care for now
        vsnprintf(buf + n, sizeof(buf) - n, ev->fmt, ev->ap);
    }
    syslog(level, "%s", buf);
}

static void file_callback(log_Event *ev)
{
    char buf[64];
    buf[strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", ev->time)] = '\0';
    fprintf(ev->udata, "%s [%i] %-5s [%s] %s:%d: ", buf, getpid(),
            level_strings[ev->level], ev->label, ev->file, ev->line);
    vfprintf(ev->udata, ev->fmt, ev->ap);
    fprintf(ev->udata, "\n");
    fflush(ev->udata);
}

static void lock(void)
{
    if (L.lock) {
        L.lock(true, L.udata);
    }
}

static void unlock(void)
{
    if (L.lock) {
        L.lock(false, L.udata);
    }
}

const char *log_level_string(int level)
{
    return level_strings[level];
}

void log_set_lock(log_LockFn fn, void *udata)
{
    L.lock  = fn;
    L.udata = udata;
}

void log_set_level(int level)
{
    L.level = level;
}

void log_set_quiet(bool enable)
{
    L.quiet = enable;
}

int log_add_callback(log_LogFn fn, void *udata, int level)
{
    for (int i = 0; i < MAX_CALLBACKS; i++) {
        if (!L.callbacks[i].fn) {
            L.callbacks[i] = (Callback) { fn, udata, level };
            return 0;
        }
    }
    return -1;
}

int log_add_fp(FILE *fp, int level)
{
    log_LogFn callback = (stdout == fp) ? stdout_callback : file_callback;
    return log_add_callback(callback, fp, level);
}

static void init_event(log_Event *ev, void *udata)
{
    if (!ev->time) {
        time_t t = time(NULL);
        ev->time = localtime(&t);
    }
    ev->udata = udata;
}

#ifdef PROJECT_ROOT_DIR
#define PRJ_ROOT_DIR_LEN sizeof(PROJECT_ROOT_DIR);
#else
#define PRJ_ROOT_DIR_LEN 0
#endif

void log_log(int level, const char *file, int line, const char *func,
             const char *label, const char *fmt, ...)
{
    const char *file_name = file + PRJ_ROOT_DIR_LEN;

    if (L.quiet || level < L.level) {
        return;
    }

    log_Event ev = { .fmt   = fmt,
                     .file  = file_name,
                     .line  = line,
                     .level = level,
                     .func  = func,
                     .label = label };

    lock();

    (void) syslog_callback; // mitigate unused warning

    for (int i = 0; i < MAX_CALLBACKS && L.callbacks[i].fn; i++) {
        Callback *cb = &L.callbacks[i];
        if (level >= cb->level) {
            init_event(&ev, cb->udata);
            va_start(ev.ap, fmt);
            cb->fn(&ev);
            va_end(ev.ap);
        }
    }

    unlock();
}
