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

/**
 * mqtt_config.c - MQTT插件配置处理
 *
 * 该文件实现了MQTT插件的配置解析和管理功能，包括：
 * 1. 解析JSON格式的插件配置
 * 2. 处理MQTT连接参数和TLS/SSL配置
 * 3. 验证配置参数的有效性
 * 4. 提供配置的初始化和清理功能
 */

#include "utils/asprintf.h"
#include "json/json.h"
#include "json/neu_json_param.h"

#include "mqtt_config.h"
#include "mqtt_plugin.h"

#define MB 1000000 // 1MB大小定义

/**
 * 解码Base64参数
 *
 * @param plugin 插件实例
 * @param el JSON参数元素，包含Base64编码的字符串
 * @return 成功返回0，失败返回-1
 */
static inline int decode_b64_param(neu_plugin_t *plugin, neu_json_elem_t *el)
{
    int   len = 0;
    char *s   = (char *) neu_decode64(&len, el->v.val_str);

    // 检查解码是否成功
    if (NULL == s) {
        plog_error(plugin, "setting %s invalid base64", el->name);
        return -1;
    }

    // 检查解码后的内容是否为空
    if (0 == len) {
        plog_error(plugin, "setting empty %s", el->name);
        free(s);
        return -1;
    }

    // 释放原始字符串，使用解码后的字符串
    free(el->v.val_str);
    el->v.val_str = s;
    return 0;
}

/**
 * 解析并解码Base64参数
 *
 * @param plugin 插件实例
 * @param setting JSON配置字符串
 * @param el 要填充的JSON参数元素
 * @return 成功返回0，失败返回-1
 */
static inline int parse_b64_param(neu_plugin_t *plugin, const char *setting,
                                  neu_json_elem_t *el)
{
    // 从配置中解析参数
    if (0 != neu_parse_param(setting, NULL, 1, el)) {
        plog_error(plugin, "setting no %s", el->name);
        return -1;
    }

    // 解码Base64参数
    if (0 != decode_b64_param(plugin, el)) {
        free(el->v.val_str);
        el->v.val_str = NULL;
        return -1;
    }

    return 0;
}

/**
 * 解析SSL/TLS参数
 *
 * @param plugin 插件实例
 * @param setting JSON配置字符串
 * @param ssl SSL启用标志参数
 * @param ca CA证书参数
 * @param cert 客户端证书参数
 * @param key 客户端私钥参数
 * @param keypass 私钥密码参数
 * @return 成功返回0，失败返回-1
 */
static inline int parse_ssl_params(neu_plugin_t *plugin, const char *setting,
                                   neu_json_elem_t *ssl, neu_json_elem_t *ca,
                                   neu_json_elem_t *cert, neu_json_elem_t *key,
                                   neu_json_elem_t *keypass)
{
    // 解析ssl参数（可选）
    int ret = neu_parse_param(setting, NULL, 1, ssl);
    if (0 != ret) {
        plog_notice(plugin, "setting no ssl");
        return 0;
    }

    // 如果SSL被禁用，直接返回
    if (false == ssl->v.val_bool) {
        plog_notice(plugin, "setting ssl disabled");
        return 0;
    }

    // 解析ca证书参数（可选）
    ret = neu_parse_param(setting, NULL, 1, ca);
    if (0 != ret) {
        plog_notice(plugin, "setting no ca");
        return 0;
    }

    // 解码CA证书（Base64格式）
    if (0 != decode_b64_param(plugin, ca)) {
        return -1;
    }

    // 解析客户端证书参数（可选）
    ret = neu_parse_param(setting, NULL, 1, cert);
    if (0 != ret) {
        plog_notice(plugin, "setting no cert");
        return 0;
    }

    // 解码客户端证书（Base64格式）
    if (0 != decode_b64_param(plugin, cert)) {
        return -1;
    }

    // 解析客户端私钥参数（如果启用了证书，则必需）
    ret = parse_b64_param(plugin, setting, key);
    if (0 != ret) {
        return -1;
    }

    // 解析私钥密码参数（可选）
    // 保留此代码以与2.5版本保持向后兼容
    ret = neu_parse_param(setting, NULL, 1, keypass);
    if (0 != ret) {
        plog_notice(plugin, "setting no keypass");
    } else if (0 == strlen(keypass->v.val_str)) {
        plog_error(plugin, "setting invalid keypass");
        return -1;
    }

    return 0;
}

/**
 * 解析缓存相关参数
 *
 * @param plugin 插件实例
 * @param setting JSON配置字符串
 * @param offline_cache 离线缓存启用标志
 * @param cache_mem_size 内存缓存大小参数
 * @param cache_disk_size 磁盘缓存大小参数
 * @param cache_sync_interval 缓存同步间隔参数
 * @return 成功返回0，失败返回非0错误码
 */
static int parse_cache_params(neu_plugin_t *plugin, const char *setting,
                              neu_json_elem_t *offline_cache,
                              neu_json_elem_t *cache_mem_size,
                              neu_json_elem_t *cache_disk_size,
                              neu_json_elem_t *cache_sync_interval)
{
    int   ret          = 0;
    char *err_param    = NULL;
    bool  flag_present = true; // 配置中是否存在`offline-cache`标志

    // 解析离线缓存标志（可选）
    ret = neu_parse_param(setting, NULL, 1, offline_cache);
    if (0 != ret) {
        plog_notice(plugin, "setting no offline cache flag");
        flag_present = false; // 配置中不存在`offline-cache`标志
    }

    if (flag_present && !offline_cache->v.val_bool) {
        // 配置中明确禁用了缓存
        cache_mem_size->v.val_int      = 0;
        cache_disk_size->v.val_int     = 0;
        cache_sync_interval->v.val_int = NEU_MQTT_CACHE_SYNC_INTERVAL_DEFAULT;
        return 0;
    }

    // 我们来到这里是因为以下其中一种情况:
    //    1. 配置中`offline-cache`设置为true
    //    2. 配置中没有`offline-cache`
    // 在这两种情况下，我们都需要`cache_mem_size`和`cache_disk_size`参数，
    // 特别是在第2种情况下，为了向后兼容性。

    // 解析内存缓存和磁盘缓存大小参数
    ret = neu_parse_param(setting, &err_param, 2, cache_mem_size,
                          cache_disk_size);
    if (0 != ret) {
        plog_error(plugin, "parsing setting fail, key: `%s`", err_param);
        free(err_param);
        return -1;
    }

    // 验证内存缓存大小，范围为1-1024MB
    if (cache_mem_size->v.val_int > 1024 ||
        (flag_present && cache_mem_size->v.val_int < 1)) {
        plog_error(plugin, "setting invalid cache memory size: %" PRIi64,
                   cache_mem_size->v.val_int);
        return -1;
    }

    // 验证磁盘缓存大小，范围为1-10240MB
    if (cache_disk_size->v.val_int > 10240 ||
        (flag_present && cache_disk_size->v.val_int < 1)) {
        plog_error(plugin, "setting invalid cache disk size: %" PRIi64,
                   cache_disk_size->v.val_int);
        return -1;
    }

    // 确保内存缓存大小不大于磁盘缓存大小
    if (cache_mem_size->v.val_int > cache_disk_size->v.val_int) {
        plog_error(plugin,
                   "setting cache memory size %" PRIi64
                   " larger than cache disk size %" PRIi64,
                   cache_mem_size->v.val_int, cache_disk_size->v.val_int);
        return -1;
    }

    // 不允许只有磁盘缓存而没有内存缓存
    if (0 == cache_mem_size->v.val_int && 0 != cache_disk_size->v.val_int) {
        plog_error(plugin,
                   "setting cache disk size %" PRIi64 " without memory cache",
                   cache_disk_size->v.val_int);
        return -1;
    }

    // 如果配置中没有离线缓存标志，根据内存缓存大小来决定是否启用
    if (!flag_present) {
        offline_cache->v.val_bool = cache_mem_size->v.val_int > 0;
    }

    // 解析缓存同步间隔参数（为了向后兼容性，此参数是可选的）
    ret = neu_parse_param(setting, NULL, 1, cache_sync_interval);
    if (0 == ret) {
        // 验证缓存同步间隔是否在有效范围内
        if (cache_sync_interval->v.val_int < NEU_MQTT_CACHE_SYNC_INTERVAL_MIN ||
            NEU_MQTT_CACHE_SYNC_INTERVAL_MAX < cache_sync_interval->v.val_int) {
            plog_error(plugin, "setting invalid cache sync interval: %" PRIi64,
                       cache_sync_interval->v.val_int);
            return -1;
        }
    } else {
        // 如果没有配置，使用默认值
        plog_notice(plugin, "setting no cache sync interval");
        cache_sync_interval->v.val_int = NEU_MQTT_CACHE_SYNC_INTERVAL_DEFAULT;
    }

    return 0;
}

/**
 * 解析MQTT插件配置
 *
 * @param plugin 插件实例
 * @param setting JSON配置字符串
 * @param config 配置结构体指针，用于存储解析结果
 * @return 成功返回0，失败返回非0错误码
 */
int mqtt_config_parse(neu_plugin_t *plugin, const char *setting,
                      mqtt_config_t *config)
{
    int         ret         = 0;
    char       *err_param   = NULL;
    const char *placeholder = "********"; // 用于密码等敏感信息的占位符

    // 定义各配置参数
    neu_json_elem_t client_id = { .name = "client-id", .t = NEU_JSON_STR };
    neu_json_elem_t qos       = {
              .name      = "qos",
              .t         = NEU_JSON_INT,
              .v.val_int = NEU_MQTT_QOS0,               // 默认使用QoS0
              .attribute = NEU_JSON_ATTRIBUTE_OPTIONAL, // 为了向后兼容性，此参数可选
    };
    neu_json_elem_t format          = { .name = "format", .t = NEU_JSON_INT };
    neu_json_elem_t write_req_topic = {
        .name      = "write-req-topic",
        .t         = NEU_JSON_STR,
        .v.val_str = NULL,
        .attribute = NEU_JSON_ATTRIBUTE_OPTIONAL, // 为了向后兼容性，此参数可选
    };
    neu_json_elem_t write_resp_topic = {
        .name      = "write-resp-topic",
        .t         = NEU_JSON_STR,
        .v.val_str = NULL,
        .attribute = NEU_JSON_ATTRIBUTE_OPTIONAL, // for backward compatibility
    };
    // 缓存相关参数
    neu_json_elem_t offline_cache       = { .name = "offline-cache",
                                            .t    = NEU_JSON_BOOL };
    neu_json_elem_t cache_mem_size      = { .name = "cache-mem-size",
                                            .t    = NEU_JSON_INT };
    neu_json_elem_t cache_disk_size     = { .name = "cache-disk-size",
                                            .t    = NEU_JSON_INT };
    neu_json_elem_t cache_sync_interval = { .name = "cache-sync-interval",
                                            .t    = NEU_JSON_INT };

    // 连接参数
    neu_json_elem_t host     = { .name = "host", .t = NEU_JSON_STR };
    neu_json_elem_t port     = { .name = "port", .t = NEU_JSON_INT };
    neu_json_elem_t username = { .name = "username", .t = NEU_JSON_STR };
    neu_json_elem_t password = { .name = "password", .t = NEU_JSON_STR };

    // SSL/TLS参数
    neu_json_elem_t ssl     = { .name = "ssl", .t = NEU_JSON_BOOL };
    neu_json_elem_t ca      = { .name = "ca", .t = NEU_JSON_STR };
    neu_json_elem_t cert    = { .name = "cert", .t = NEU_JSON_STR };
    neu_json_elem_t key     = { .name = "key", .t = NEU_JSON_STR };
    neu_json_elem_t keypass = { .name = "keypass", .t = NEU_JSON_STR };

    // 验证参数
    if (NULL == setting || NULL == config) {
        plog_error(plugin, "invalid argument, null pointer");
        return -1;
    }

    // 解析基本参数
    ret = neu_parse_param(setting, &err_param, 7, &client_id, &qos, &format,
                          &write_req_topic, &write_resp_topic, &host, &port);
    if (0 != ret) {
        plog_error(plugin, "parsing setting fail, key: `%s`", err_param);
        goto error;
    }

    // 验证客户端ID（必需）
    if (0 == strlen(client_id.v.val_str)) {
        plog_error(plugin, "setting empty client-id");
        goto error;
    }

    // 验证QoS参数值范围（可选，默认为QoS0）
    if (qos.v.val_int < NEU_MQTT_QOS0 || NEU_MQTT_QOS2 < qos.v.val_int) {
        plog_error(plugin, "setting invalid qos: %" PRIi64, qos.v.val_int);
        goto error;
    }

    // 验证数据格式参数（必需）
    if (MQTT_UPLOAD_FORMAT_VALUES != format.v.val_int &&
        MQTT_UPLOAD_FORMAT_TAGS != format.v.val_int) {
        plog_error(plugin, "setting invalid format: %" PRIi64,
                   format.v.val_int);
        goto error;
    }

    // 处理写请求主题（如果未指定，生成默认主题）
    if (NULL == write_req_topic.v.val_str &&
        0 > neu_asprintf(&write_req_topic.v.val_str, "/neuron/%s/write/req",
                         plugin->common.name)) {
        plog_error(plugin, "setting write request topic error");
        goto error;
    }

    // 处理写响应主题（如果未指定，生成默认主题）
    if (NULL == write_resp_topic.v.val_str &&
        0 > neu_asprintf(&write_resp_topic.v.val_str, "/neuron/%s/write/resp",
                         plugin->common.name)) {
        plog_error(plugin, "setting write response topic error");
        goto error;
    }

    // 解析离线缓存参数
    ret = parse_cache_params(plugin, setting, &offline_cache, &cache_mem_size,
                             &cache_disk_size, &cache_sync_interval);
    if (0 != ret) {
        goto error;
    }

    // 验证主机地址（必需）
    if (0 == strlen(host.v.val_str)) {
        plog_error(plugin, "setting invalid host: `%s`", host.v.val_str);
        goto error;
    }

    // 验证端口号（必需且在有效范围内）
    if (0 == port.v.val_int || port.v.val_int > 65535) {
        plog_error(plugin, "setting invalid port: %" PRIi64, port.v.val_int);
        goto error;
    }

    // 解析用户名参数（可选）
    ret = neu_parse_param(setting, NULL, 1, &username);
    if (0 != ret) {
        plog_notice(plugin, "setting no username");
    }

    // 解析密码参数（可选）
    ret = neu_parse_param(setting, NULL, 1, &password);
    if (0 != ret) {
        plog_notice(plugin, "setting no password");
    }

    // 解析SSL/TLS相关参数
    ret = parse_ssl_params(plugin, setting, &ssl, &ca, &cert, &key, &keypass);
    if (0 != ret) {
        goto error;
    }

    // 填充配置结构体
    config->client_id        = client_id.v.val_str;        // 客户端ID
    config->qos              = qos.v.val_int;              // 服务质量
    config->format           = format.v.val_int;           // 数据格式
    config->write_req_topic  = write_req_topic.v.val_str;  // 写请求主题
    config->write_resp_topic = write_resp_topic.v.val_str; // 写响应主题
    config->cache            = offline_cache.v.val_bool;   // 离线缓存开关
    config->cache_mem_size =
        cache_mem_size.v.val_int * MB; // 内存缓存大小(MB转字节)
    config->cache_disk_size =
        cache_disk_size.v.val_int * MB; // 磁盘缓存大小(MB转字节)
    config->cache_sync_interval = cache_sync_interval.v.val_int; // 缓存同步间隔
    config->host                = host.v.val_str;                // 服务器地址
    config->port                = port.v.val_int;                // 服务器端口
    config->username            = username.v.val_str;            // 用户名
    config->password            = password.v.val_str;            // 密码
    config->ssl                 = ssl.v.val_bool;                // SSL/TLS开关
    config->ca                  = ca.v.val_str;                  // CA证书
    config->cert                = cert.v.val_str;                // 客户端证书
    config->key                 = key.v.val_str;                 // 客户端私钥
    config->keypass             = keypass.v.val_str;             // 私钥密码

    // 记录配置信息到日志
    plog_notice(plugin, "config client-id       : %s", config->client_id);
    plog_notice(plugin, "config qos             : %d", config->qos);
    plog_notice(plugin, "config format          : %s",
                mqtt_upload_format_str(config->format));
    plog_notice(plugin, "config write-req-topic : %s", config->write_req_topic);
    plog_notice(plugin, "config write-resp-topic: %s",
                config->write_resp_topic);
    plog_notice(plugin, "config cache           : %zu", config->cache);
    plog_notice(plugin, "config cache-mem-size  : %zu", config->cache_mem_size);
    plog_notice(plugin, "config cache-disk-size : %zu",
                config->cache_disk_size);
    plog_notice(plugin, "config cache-sync-interval : %zu",
                config->cache_sync_interval);
    plog_notice(plugin, "config host            : %s", config->host);
    plog_notice(plugin, "config port            : %" PRIu16, config->port);

    // 记录认证信息（如果有）
    if (config->username) {
        plog_notice(plugin, "config username        : %s", config->username);
    }
    if (config->password) {
        // 密码使用占位符保护，不直接输出到日志
        plog_notice(plugin, "config password        : %s",
                    0 == strlen(config->password) ? "" : placeholder);
    }

    // 记录SSL/TLS配置
    plog_notice(plugin, "config ssl             : %d", config->ssl);
    // 证书和密钥信息使用占位符保护，不输出实际内容到日志
    if (config->ca) {
        plog_notice(plugin, "config ca              : %s", placeholder);
    }
    if (config->cert) {
        plog_notice(plugin, "config cert            : %s", placeholder);
    }
    if (config->key) {
        plog_notice(plugin, "config key             : %s", placeholder);
    }
    if (config->keypass) {
        plog_notice(plugin, "config keypass         : %s", placeholder);
    }

    return 0;

error:
    // 错误处理：释放已分配的内存
    free(err_param);
    free(client_id.v.val_str);
    free(write_req_topic.v.val_str);
    free(write_resp_topic.v.val_str);
    free(host.v.val_str);
    free(username.v.val_str);
    free(password.v.val_str);
    free(ca.v.val_str);
    free(cert.v.val_str);
    free(key.v.val_str);
    free(keypass.v.val_str);
    return -1;
}

/**
 * 释放MQTT配置结构体资源
 *
 * @param config 配置结构体指针
 */
void mqtt_config_fini(mqtt_config_t *config)
{
    // 释放所有分配的字符串资源
    free(config->client_id);
    free(config->write_req_topic);
    free(config->write_resp_topic);
    free(config->host);
    free(config->username);
    free(config->password);
    free(config->ca);
    free(config->cert);
    free(config->key);
    free(config->keypass);

    // 清空配置结构体
    memset(config, 0, sizeof(*config));
}
