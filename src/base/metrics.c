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
#include <pthread.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <unistd.h>

#include "adapter.h"
#include "adapter/adapter_internal.h"
#include "metrics.h"
#include "utils/log.h"
#include "utils/time.h"

/**
 * @brief 度量指标的读写锁，保护全局度量数据的并发访问
 */
static pthread_rwlock_t g_metrics_mtx_ = PTHREAD_RWLOCK_INITIALIZER;

/**
 * @brief 全局度量指标结构体，存储各种系统和应用指标数据
 */
static neu_metrics_t g_metrics_;

/**
 * @brief 应用启动时间戳，用于计算运行时间
 */
static uint64_t g_start_ts_;

/**
 * @brief 获取操作系统信息
 *
 * 执行系统命令获取操作系统发行版和内核版本信息
 */
static void find_os_info()
{
    /* 构建命令：从/etc/os-release获取系统信息，如果不存在则使用uname */
    const char *cmd = "if [ -f /etc/os-release ]; then . /etc/os-release;"
                      "echo $NAME $VERSION_ID; else uname -s; fi; uname -r";
    FILE       *f   = popen(cmd, "r");

    if (NULL == f) {
        nlog_error("popen command fail");
        return;
    }

    char buf[64] = {};

    /* 获取发行版信息 */
    if (NULL == fgets(buf, sizeof(buf), f)) {
        nlog_error("no command output");
        pclose(f);
        return;
    }
    /* 移除换行符并复制到度量结构体中 */
    buf[strcspn(buf, "\n")] = 0;
    strncpy(g_metrics_.distro, buf, sizeof(g_metrics_.distro));
    g_metrics_.distro[sizeof(g_metrics_.distro) - 1] =
        0; /* 确保字符串以NULL结尾 */

    /* 获取内核版本信息 */
    if (NULL == fgets(buf, sizeof(buf), f)) {
        nlog_error("no command output");
        pclose(f);
        return;
    }
    /* 移除换行符并复制到度量结构体中 */
    buf[strcspn(buf, "\n")] = 0;
    strncpy(g_metrics_.kernel, buf, sizeof(g_metrics_.kernel));
    g_metrics_.kernel[sizeof(g_metrics_.kernel) - 1] =
        0; /* 确保字符串以NULL结尾 */

    pclose(f);
}

/**
 * @brief 解析内存字段值
 *
 * 通过执行free命令并提取特定列的值来获取内存使用情况
 *
 * @param col 要提取的free命令输出的列号
 * @return 提取的内存值(字节)
 */
static size_t parse_memory_fields(int col)
{
    FILE  *f       = NULL;
    char   buf[64] = {};
    size_t val     = 0;

    /* 构建命令：获取free命令输出的特定列（以字节为单位） */
    sprintf(buf, "free -b | awk 'NR==2 {print $%i}'", col);

    f = popen(buf, "r");
    if (NULL == f) {
        nlog_error("popen command fail");
        return 0;
    }

    /* 解析命令输出的数值 */
    if (NULL != fgets(buf, sizeof(buf), f)) {
        val = atoll(buf);
    } else {
        nlog_error("no command output");
    }

    pclose(f);
    return val;
}

/**
 * @brief 获取系统总内存大小
 *
 * @return 总内存大小(字节)
 */
static inline size_t memory_total()
{
    return parse_memory_fields(2); /* free命令输出的第2列是总内存 */
}

/**
 * @brief 获取系统已使用内存大小
 *
 * @return 已使用内存大小(字节)
 */
static inline size_t memory_used()
{
    return parse_memory_fields(3); /* free命令输出的第3列是已用内存 */
}

/**
 * @brief 获取Neuron进程使用的内存
 *
 * 通过ps命令获取当前进程的内存使用情况
 *
 * @return Neuron进程使用的内存大小(字节)
 */
static inline size_t neuron_memory_used()
{
    FILE  *f       = NULL;
    char   buf[32] = {};
    size_t val     = 0;
    pid_t  pid     = getpid(); /* 获取当前进程ID */

    /* 构建命令：获取当前进程的RSS值(驻留集大小) */
    sprintf(buf, "ps -o rss= %ld", (long) pid);

    f = popen(buf, "r");
    if (NULL == f) {
        nlog_error("popen command fail");
        return 0;
    }

    /* 解析命令输出的RSS值 */
    if (NULL != fgets(buf, sizeof(buf), f)) {
        val = atoll(buf);
    } else {
        nlog_error("no command output");
    }

    pclose(f);
    return val * 1024; /* ps命令输出的RSS单位是KB，转换为字节 */
}

/**
 * @brief 获取系统缓存大小
 *
 * @return 缓存内存大小(字节)
 */
static inline size_t memory_cache()
{
    return parse_memory_fields(6); /* free命令输出的第6列是缓存大小 */
}

/**
 * @brief 获取当前目录所在磁盘的使用情况
 *
 * 通过statvfs系统调用获取磁盘总大小、已使用和可用空间
 *
 * @param size_p 用于返回磁盘总大小(GB)
 * @param used_p 用于返回已使用空间(GB)
 * @param avail_p 用于返回可用空间(GB)
 * @return 成功返回0，失败返回-1
 */
static inline int disk_usage(size_t *size_p, size_t *used_p, size_t *avail_p)
{
    struct statvfs buf = {};
    /* 获取当前目录的文件系统信息 */
    if (0 != statvfs(".", &buf)) {
        return -1;
    }

    /* 计算磁盘大小，单位转换为GB */
    *size_p  = (double) buf.f_frsize * buf.f_blocks / (1 << 30);
    *used_p  = (double) buf.f_frsize * (buf.f_blocks - buf.f_bfree) / (1 << 30);
    *avail_p = (double) buf.f_frsize * buf.f_bavail / (1 << 30);
    return 0;
}

/**
 * @brief 获取CPU使用率
 *
 * 通过两次读取/proc/stat中的CPU时间数据，计算CPU使用率
 *
 * @return CPU使用率百分比(0-100)
 */
static unsigned cpu_usage()
{
    int ret = 0;
    /* 第一次采样的CPU时间数据 */
    unsigned long long user1 = 0, nice1 = 0, sys1 = 0, idle1 = 0, iowait1 = 0,
                       irq1 = 0, softirq1 = 0;
    /* 第二次采样的CPU时间数据 */
    unsigned long long user2 = 0, nice2 = 0, sys2 = 0, idle2 = 0, iowait2 = 0,
                       irq2 = 0, softirq2 = 0;
    unsigned long long work = 0, total = 0;
    /* 两次采样间的时间间隔(50毫秒) */
    struct timespec tv = {
        .tv_sec  = 0,
        .tv_nsec = 50000000,
    };
    FILE *f = NULL;

    /* 打开/proc/stat文件 */
    f = fopen("/proc/stat", "r");
    if (NULL == f) {
        nlog_error("open /proc/stat fail");
        return 0;
    }

    /* 第一次读取CPU时间数据 */
    ret = fscanf(f, "cpu %llu %llu %llu %llu %llu %llu %llu", &user1, &nice1,
                 &sys1, &idle1, &iowait1, &irq1, &softirq1);
    if (7 != ret) {
        fclose(f);
        return 0;
    }

    /* 休眠50毫秒，等待CPU状态变化 */
    nanosleep(&tv, NULL);

    /* 重新定位文件指针到文件开头 */
    rewind(f);
    /* 第二次读取CPU时间数据 */
    ret = fscanf(f, "cpu %llu %llu %llu %llu %llu %llu %llu", &user2, &nice2,
                 &sys2, &idle2, &iowait2, &irq2, &softirq2);
    if (7 != ret) {
        fclose(f);
        return 0;
    }
    fclose(f);

    /* 计算工作时间(user+nice+system)的变化量 */
    work = (user2 - user1) + (nice2 - nice1) + (sys2 - sys1);
    /* 计算总时间的变化量 */
    total = work + (idle2 - idle1) + (iowait2 - iowait1) + (irq2 - irq1) +
        (softirq2 - softirq1);

    /* 计算CPU使用率百分比，并乘以CPU核心数 */
    return (double) work / total * 100.0 * sysconf(_SC_NPROCESSORS_CONF);
}

/**
 * @brief 检查指定目录中是否存在以特定前缀开头的核心转储文件
 *
 * @param dir 要检查的目录路径
 * @param prefix 核心转储文件名前缀
 * @return 如果找到核心转储文件则返回true，否则返回false
 */
static bool has_core_dump_in_dir(const char *dir, const char *prefix)
{
    /* 打开指定目录 */
    DIR *dp = opendir(dir);
    if (dp == NULL) {
        return false;
    }

    /* 遍历目录中的文件 */
    struct dirent *de;
    bool           found = false;
    while ((de = readdir(dp)) != NULL) {
        /* 检查文件名是否以指定前缀开头 */
        if (strncmp(prefix, de->d_name, strlen(prefix)) == 0) {
            found = true;
            break;
        }
    }

    closedir(dp);
    return found;
}

/**
 * @brief 从核心转储模式字符串中提取目录路径
 *
 * 从/proc/sys/kernel/core_pattern文件内容中提取核心转储文件的目录路径
 *
 * @param core_pattern 核心转储模式字符串
 * @return 提取的目录路径，如果没有目录路径则返回NULL
 */
static char *get_core_dir(const char *core_pattern)
{
    /* 查找最后一个斜杠位置，用于分离目录路径 */
    char *last_slash = strrchr(core_pattern, '/');
    if (last_slash != NULL) {
        static char core_dir[256];
        ptrdiff_t   path_length = last_slash - core_pattern + 1;
        /* 复制目录部分到core_dir */
        strncpy(core_dir, core_pattern, path_length);
        core_dir[path_length] = '\0';
        return core_dir;
    }
    return NULL;
}

/**
 * @brief 检查系统中是否存在Neuron的核心转储文件
 *
 * 首先检查当前目录的core文件夹，然后根据系统核心转储配置检查其他可能的位置
 *
 * @return 如果找到核心转储文件则返回true，否则返回false
 */
static bool has_core_dumps()
{
    /* 首先检查当前目录下的core目录 */
    if (has_core_dump_in_dir("core", "core-neuron")) {
        return true;
    }

    /* 读取系统核心转储模式配置 */
    FILE *fp = fopen("/proc/sys/kernel/core_pattern", "r");
    if (fp == NULL) {
        return false;
    }

    /* 读取core_pattern内容 */
    char core_pattern[256];
    if (fgets(core_pattern, sizeof(core_pattern), fp) == NULL) {
        fclose(fp);
        return false;
    }
    fclose(fp);

    /* 确定核心转储文件的目录
     * 如果core_pattern以'|'开头，表示使用管道处理核心转储，
     * 通常Ubuntu系统会将核心转储存储在/var/crash/目录
     */
    char *core_dir =
        (core_pattern[0] == '|') ? "/var/crash/" : get_core_dir(core_pattern);
    if (core_dir == NULL) {
        return false;
    }

    /* 在核心转储目录中查找neuron的核心转储文件 */
    return has_core_dump_in_dir(core_dir, "core-neuron");
}

/**
 * @brief 注销度量指标条目
 *
 * 从全局度量指标哈希表中注销一个条目，当引用计数为0时删除该条目
 *
 * @param name 要注销的度量指标名称
 */
static inline void metrics_unregister_entry(const char *name)
{
    neu_metric_entry_t *e = NULL;
    /* 在哈希表中查找指定名称的条目 */
    HASH_FIND_STR(g_metrics_.registered_metrics, name, e);
    /* 递减引用计数，当计数为0时删除条目 */
    if (0 == --e->value) {
        HASH_DEL(g_metrics_.registered_metrics, e);
        nlog_notice("del entry:%s", e->name);
        neu_metric_entry_free(e);
    }
}

/**
 * @brief 向度量指标集合添加一个新的度量指标条目
 *
 * @param entries 度量指标集合的哈希表指针
 * @param name 度量指标名称
 * @param help 度量指标的帮助说明文本
 * @param type 度量指标类型(计数器、仪表等)
 * @param init 度量指标的初始值
 * @return 成功返回0，指标已存在返回1，失败返回-1
 */
int neu_metric_entries_add(neu_metric_entry_t **entries, const char *name,
                           const char *help, neu_metric_type_e type,
                           uint64_t init)
{
    neu_metric_entry_t *entry = NULL;
    /* 检查是否已存在同名的度量指标 */
    HASH_FIND_STR(*entries, name, entry);
    if (NULL != entry) {
        /* 如果已存在但类型或帮助文本不匹配，则报错 */
        if (entry->type != type || (0 != strcmp(entry->help, help))) {
            nlog_error("metric entry %s(%d, %s) conflicts with (%d, %s)", name,
                       entry->type, entry->help, type, help);
            return -1;
        }
        return 1;
    }

    /* 创建新的度量指标条目 */
    entry = calloc(1, sizeof(*entry));
    if (NULL == entry) {
        return -1;
    }

    /* 根据指标类型进行初始化 */
    if (NEU_METRIC_TYPE_ROLLING_COUNTER == type) {
        // 只为非零时间跨度分配滚动计数器
        if (init > 0 && NULL == (entry->rcnt = neu_rolling_counter_new(init))) {
            free(entry);
            return -1;
        }
    } else {
        /* 对于其他类型，直接设置初始值 */
        entry->value = init;
    }

    /* 设置度量指标的属性 */
    entry->name = name;
    entry->type = type;
    entry->help = help;
    /* 将新的度量指标添加到哈希表 */
    HASH_ADD_STR(*entries, name, entry);
    return 0;
}

/**
 * @brief 初始化度量指标系统
 *
 * 初始化全局度量指标相关变量，获取系统信息和内存总量
 */
void neu_metrics_init()
{
    /* 加写锁，确保初始化过程的线程安全 */
    pthread_rwlock_wrlock(&g_metrics_mtx_);
    if (0 == g_start_ts_) {
        /* 记录启动时间戳 */
        g_start_ts_ = neu_time_ms();
        /* 获取操作系统信息 */
        find_os_info();
        /* 获取系统内存总量 */
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
        } else if (NEU_NA_TYPE_APP == n->adapter->module->type ||
                   NEU_NA_TYPE_NDRIVER == n->adapter->module->type) {
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
