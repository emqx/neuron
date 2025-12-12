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

#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <unistd.h>

#ifndef NEU_CLIB
#include <gnu/libc-version.h>
#endif

#include "adapter.h"
#include "adapter/adapter_internal.h"
#include "metrics.h"
#include "utils/log.h"
#include "utils/time.h"

pthread_rwlock_t g_metrics_mtx_ = PTHREAD_RWLOCK_INITIALIZER;
neu_metrics_t    g_metrics_;
static uint64_t  g_start_ts_;

static void find_os_info()
{
    const char *cmd =
        "if [ -f /etc/os-release ]; then . /etc/os-release;"
        "echo $NAME $VERSION_ID; else uname -s; fi; uname -r; uname -m";
    FILE *f = popen(cmd, "r");

    if (NULL == f) {
        nlog_error("popen command fail(%s)", strerror(errno));
        return;
    }

    char buf[64] = {};

    if (NULL == fgets(buf, sizeof(buf), f)) {
        nlog_error("no command output");
        pclose(f);
        return;
    }
    buf[strcspn(buf, "\n")] = 0;
    strncpy(g_metrics_.distro, buf, sizeof(g_metrics_.distro));
    g_metrics_.distro[sizeof(g_metrics_.distro) - 1] = 0;

    if (NULL == fgets(buf, sizeof(buf), f)) {
        nlog_error("no command output");
        pclose(f);
        return;
    }
    buf[strcspn(buf, "\n")] = 0;
    strncpy(g_metrics_.kernel, buf, sizeof(g_metrics_.kernel));
    g_metrics_.kernel[sizeof(g_metrics_.kernel) - 1] = 0;

    if (NULL == fgets(buf, sizeof(buf), f)) {
        nlog_error("no command output");
        pclose(f);
        return;
    }
    buf[strcspn(buf, "\n")] = 0;
    strncpy(g_metrics_.machine, buf, sizeof(g_metrics_.machine));
    g_metrics_.kernel[sizeof(g_metrics_.machine) - 1] = 0;

    pclose(f);

#ifdef NEU_CLIB
    strncpy(g_metrics_.clib, NEU_CLIB, sizeof(g_metrics_.clib));
    strncpy(g_metrics_.clib_version, "unknow", sizeof(g_metrics_.clib_version));
#else
    strncpy(g_metrics_.clib, "glibc", sizeof(g_metrics_.clib));
    strncpy(g_metrics_.clib_version, gnu_get_libc_version(),
            sizeof(g_metrics_.clib_version));
#endif
}

static size_t parse_memory_fields(int col)
{
    FILE * f       = NULL;
    char   buf[64] = {};
    size_t val     = 0;

    sprintf(buf, "free -b | awk 'NR==2 {print $%i}'", col);

    f = popen(buf, "r");
    if (NULL == f) {
        nlog_error("popen command fail(%s)", strerror(errno));
        return 0;
    }

    if (NULL != fgets(buf, sizeof(buf), f)) {
        val = atoll(buf);
    } else {
        nlog_error("no command output");
    }

    pclose(f);
    return val;
}

static inline size_t memory_total()
{
    return parse_memory_fields(2);
}

static inline size_t memory_used()
{
    return parse_memory_fields(3);
}

static inline size_t neuron_memory_used()
{
    FILE * f       = NULL;
    char   buf[32] = {};
    size_t val     = 0;
    pid_t  pid     = getpid();

    sprintf(buf, "ps -o rss= %ld", (long) pid);

    f = popen(buf, "r");
    if (NULL == f) {
        nlog_error("popen command fail(%s)", strerror(errno));
        return 0;
    }

    if (NULL != fgets(buf, sizeof(buf), f)) {
        val = atoll(buf);
    } else {
        nlog_error("no command output");
    }

    pclose(f);
    return val * 1024;
}

static inline size_t memory_cache()
{
    return parse_memory_fields(6);
}

static inline int disk_usage(size_t *size_p, size_t *used_p, size_t *avail_p)
{
    struct statvfs buf = {};
    if (0 != statvfs(".", &buf)) {
        return -1;
    }

    *size_p  = (double) buf.f_frsize * buf.f_blocks / (1 << 30);
    *used_p  = (double) buf.f_frsize * (buf.f_blocks - buf.f_bfree) / (1 << 30);
    *avail_p = (double) buf.f_frsize * buf.f_bavail / (1 << 30);
    return 0;
}

static unsigned cpu_usage()
{
    int                ret   = 0;
    unsigned long long user1 = 0, nice1 = 0, sys1 = 0, idle1 = 0, iowait1 = 0,
                       irq1 = 0, softirq1 = 0;
    unsigned long long user2 = 0, nice2 = 0, sys2 = 0, idle2 = 0, iowait2 = 0,
                       irq2 = 0, softirq2 = 0;
    unsigned long long work = 0, total = 0;
    struct timespec    tv = {
        .tv_sec  = 0,
        .tv_nsec = 50000000,
    };
    FILE *f = NULL;

    f = fopen("/proc/stat", "r");
    if (NULL == f) {
        nlog_error("open /proc/stat fail");
        return 0;
    }

    ret = fscanf(f, "cpu %llu %llu %llu %llu %llu %llu %llu", &user1, &nice1,
                 &sys1, &idle1, &iowait1, &irq1, &softirq1);
    if (7 != ret) {
        fclose(f);
        return 0;
    }

    nanosleep(&tv, NULL);

    rewind(f);
    ret = fscanf(f, "cpu %llu %llu %llu %llu %llu %llu %llu", &user2, &nice2,
                 &sys2, &idle2, &iowait2, &irq2, &softirq2);
    if (7 != ret) {
        fclose(f);
        return 0;
    }
    fclose(f);

    work  = (user2 - user1) + (nice2 - nice1) + (sys2 - sys1);
    total = work + (idle2 - idle1) + (iowait2 - iowait1) + (irq2 - irq1) +
        (softirq2 - softirq1);

    return (double) work / total * 100.0 * sysconf(_SC_NPROCESSORS_CONF);
}

static bool has_core_dump_in_dir(const char *dir, const char *prefix)
{
    DIR *dp = opendir(dir);
    if (dp == NULL) {
        return false;
    }

    struct dirent *de;
    bool           found = false;
    while ((de = readdir(dp)) != NULL) {
        if (strncmp(prefix, de->d_name, strlen(prefix)) == 0) {
            found = true;
            break;
        }
    }

    closedir(dp);
    return found;
}

static char *get_core_dir(const char *core_pattern)
{
    char *last_slash = strrchr(core_pattern, '/');
    if (last_slash != NULL) {
        static char core_dir[256];
        ptrdiff_t   path_length = last_slash - core_pattern + 1;
        strncpy(core_dir, core_pattern, path_length);
        core_dir[path_length] = '\0';
        return core_dir;
    }
    return NULL;
}

static bool has_core_dumps()
{
    if (has_core_dump_in_dir("core", "core-neuron")) {
        return true;
    }

    FILE *fp = fopen("/proc/sys/kernel/core_pattern", "r");
    if (fp == NULL) {
        return false;
    }

    char core_pattern[256];
    if (fgets(core_pattern, sizeof(core_pattern), fp) == NULL) {
        fclose(fp);
        return false;
    }
    fclose(fp);

    char *core_dir =
        (core_pattern[0] == '|') ? "/var/crash/" : get_core_dir(core_pattern);
    if (core_dir == NULL) {
        return false;
    }

    return has_core_dump_in_dir(core_dir, "core-neuron");
}

static inline void metrics_unregister_entry(const char *name)
{
    neu_metric_entry_t *e = NULL;
    HASH_FIND_STR(g_metrics_.registered_metrics, name, e);
    if (0 == --e->value) {
        HASH_DEL(g_metrics_.registered_metrics, e);
        nlog_notice("del entry:%s", e->name);
        neu_metric_entry_free(e);
    }
}

int neu_metric_entries_add(neu_metric_entry_t **entries, const char *name,
                           const char *help, neu_metric_type_e type,
                           uint64_t init)
{
    neu_metric_entry_t *entry = NULL;
    HASH_FIND_STR(*entries, name, entry);
    if (NULL != entry) {
        if (entry->type != type || (0 != strcmp(entry->help, help))) {
            nlog_error("metric entry %s(%d, %s) conflicts with (%d, %s)", name,
                       entry->type, entry->help, type, help);
            return -1;
        }
        return 1;
    }

    entry = calloc(1, sizeof(*entry));
    if (NULL == entry) {
        return -1;
    }

    if (NEU_METRIC_TYPE_ROLLING_COUNTER == type) {
        // only allocate rolling counter for nonzero time span
        if (init > 0 && NULL == (entry->rcnt = neu_rolling_counter_new(init))) {
            free(entry);
            return -1;
        }
    } else {
        entry->value = init;
    }

    entry->init = init;
    entry->name = name;
    entry->type = type;
    entry->help = help;
    HASH_ADD_STR(*entries, name, entry);
    return 0;
}

void neu_metrics_init()
{
    pthread_rwlock_wrlock(&g_metrics_mtx_);
    if (0 == g_start_ts_) {
        g_start_ts_ = neu_time_ms();
        find_os_info();
        g_metrics_.mem_total_bytes = memory_total();
    }
    pthread_rwlock_unlock(&g_metrics_mtx_);
}

void neu_metrics_add_node(const neu_adapter_t *adapter)
{
    pthread_rwlock_wrlock(&g_metrics_mtx_);
    HASH_ADD_STR(g_metrics_.node_metrics, name, adapter->metrics);
    pthread_rwlock_unlock(&g_metrics_mtx_);
}

void neu_metrics_del_node(const neu_adapter_t *adapter)
{
    pthread_rwlock_wrlock(&g_metrics_mtx_);
    HASH_DEL(g_metrics_.node_metrics, adapter->metrics);
    pthread_rwlock_unlock(&g_metrics_mtx_);
}

int neu_metrics_register_entry(const char *name, const char *help,
                               neu_metric_type_e type)
{
    int                 rv = 0;
    neu_metric_entry_t *e  = NULL;

    pthread_rwlock_wrlock(&g_metrics_mtx_);
    // use `value` field as reference counter, initialize to zero
    // and we don't need to allocate rolling counter for register entries
    rv = neu_metric_entries_add(&g_metrics_.registered_metrics, name, help,
                                type, 0);
    if (-1 != rv) {
        HASH_FIND_STR(g_metrics_.registered_metrics, name, e);
        ++e->value;
        rv = 0;
    }
    pthread_rwlock_unlock(&g_metrics_mtx_);
    return rv;
}

void neu_metrics_unregister_entry(const char *name)
{
    pthread_rwlock_wrlock(&g_metrics_mtx_);
    metrics_unregister_entry(name);
    pthread_rwlock_unlock(&g_metrics_mtx_);
}

void neu_metrics_visist(neu_metrics_cb_t cb, void *data)
{
    unsigned cpu       = cpu_usage();
    size_t   mem_used  = neuron_memory_used();
    size_t   mem_cache = memory_cache();
    size_t   disk_size = 0, disk_used = 0, disk_avail = 0;
    disk_usage(&disk_size, &disk_used, &disk_avail);
    bool     core_dumped    = has_core_dumps();
    uint64_t uptime_seconds = (neu_time_ms() - g_start_ts_) / 1000;
    pthread_rwlock_rdlock(&g_metrics_mtx_);
    g_metrics_.cpu_percent          = cpu;
    g_metrics_.cpu_cores            = get_nprocs();
    g_metrics_.mem_used_bytes       = mem_used;
    g_metrics_.mem_cache_bytes      = mem_cache;
    g_metrics_.disk_size_gibibytes  = disk_size;
    g_metrics_.disk_used_gibibytes  = disk_used;
    g_metrics_.disk_avail_gibibytes = disk_avail;
    g_metrics_.core_dumped          = core_dumped;
    g_metrics_.uptime_seconds       = uptime_seconds;

    g_metrics_.north_nodes              = 0;
    g_metrics_.north_running_nodes      = 0;
    g_metrics_.north_disconnected_nodes = 0;
    g_metrics_.south_nodes              = 0;
    g_metrics_.south_running_nodes      = 0;
    g_metrics_.south_disconnected_nodes = 0;

    neu_node_metrics_t *n;
    HASH_LOOP(hh, g_metrics_.node_metrics, n)
    {
        neu_plugin_common_t *common =
            neu_plugin_to_plugin_common(n->adapter->plugin);

        n->adapter->cb_funs.update_metric(n->adapter, NEU_METRIC_RUNNING_STATE,
                                          n->adapter->state, NULL);
        n->adapter->cb_funs.update_metric(n->adapter, NEU_METRIC_LINK_STATE,
                                          common->link_state, NULL);

        if (NEU_NA_TYPE_DRIVER == n->adapter->module->type) {
            ++g_metrics_.south_nodes;
            if (NEU_NODE_RUNNING_STATE_RUNNING == n->adapter->state) {
                ++g_metrics_.south_running_nodes;
            }
            if (NEU_NODE_LINK_STATE_DISCONNECTED == common->link_state) {
                ++g_metrics_.south_disconnected_nodes;
            }
        } else if (NEU_NA_TYPE_APP == n->adapter->module->type) {
            ++g_metrics_.north_nodes;
            if (NEU_NODE_RUNNING_STATE_RUNNING == n->adapter->state) {
                ++g_metrics_.north_running_nodes;
            }
            if (NEU_NODE_LINK_STATE_DISCONNECTED == common->link_state) {
                ++g_metrics_.north_disconnected_nodes;
            }
        }
    }

    cb(&g_metrics_, data);
    pthread_rwlock_unlock(&g_metrics_mtx_);
}

neu_metrics_t *neu_get_global_metrics()
{
    return &g_metrics_;
}