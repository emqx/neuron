/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2024 EMQ Technologies Co., Ltd All rights reserved.
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

#include <inttypes.h>
#include <librdkafka/rdkafka.h>
#include <time.h>

#include "errcodes.h"
#include "utils/asprintf.h"

#include "kafka_config.h"
#include "kafka_handle.h"
#include "kafka_plugin.h"
#include "kafka_plugin_intf.h"

extern const neu_plugin_module_t neu_plugin_module;

/* ----------------------------- rdkafka callbacks -------------------------- */

static void dr_msg_cb(rd_kafka_t *rk, const rd_kafka_message_t *msg,
                      void *opaque)
{
    (void) rk;
    neu_plugin_t *plugin = (neu_plugin_t *) opaque;

    if (msg->err) {
        plugin->delivery_fail++;
        NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_SEND_MSG_ERRORS_TOTAL, 1,
                                 NULL);
        if (plugin->delivery_fail % 100 == 1) {
            plog_warn(plugin,
                      "delivery failed (%s): %s (total_fail=%" PRId64 ")",
                      rd_kafka_topic_name(msg->rkt), rd_kafka_err2str(msg->err),
                      plugin->delivery_fail);
        }
    } else {
        plugin->delivery_succ++;
        NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_SEND_MSGS_TOTAL, 1, NULL);
        if (!plugin->connected) {
            plugin->connected         = true;
            plugin->common.link_state = NEU_NODE_LINK_STATE_CONNECTED;
            plog_notice(plugin,
                        "broker reconnected (delivery ok, succ=%" PRId64 ")",
                        plugin->delivery_succ);
        }
    }
}

static void error_cb(rd_kafka_t *rk, int err, const char *reason, void *opaque)
{
    (void) rk;
    neu_plugin_t *plugin = (neu_plugin_t *) opaque;

    if (err == RD_KAFKA_RESP_ERR__ALL_BROKERS_DOWN ||
        err == RD_KAFKA_RESP_ERR__TRANSPORT) {
        if (plugin->connected) {
            plugin->connected         = false;
            plugin->common.link_state = NEU_NODE_LINK_STATE_DISCONNECTED;
            NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_DISCONNECTION_60S, 1,
                                     NULL);
            NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_DISCONNECTION_600S, 1,
                                     NULL);
            NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_DISCONNECTION_1800S, 1,
                                     NULL);
            plog_error(plugin, "broker connection lost: %s", reason);
        }
    } else {
        plog_warn(plugin, "rdkafka error %d: %s", err, reason);
    }
}

static void stats_cb_check_connected(neu_plugin_t *plugin, rd_kafka_t *rk)
{
    (void) rk;
    const rd_kafka_metadata_t *md = NULL;
    rd_kafka_resp_err_t err = rd_kafka_metadata(plugin->rk, 0, NULL, &md, 2000);

    if (err == RD_KAFKA_RESP_ERR_NO_ERROR && md && md->broker_cnt > 0) {
        if (!plugin->connected) {
            plugin->connected         = true;
            plugin->common.link_state = NEU_NODE_LINK_STATE_CONNECTED;
            plog_notice(plugin, "broker connected (brokers=%d)",
                        md->broker_cnt);
        }
    }

    if (md) {
        rd_kafka_metadata_destroy(md);
    }
}

/* ------------------------------ poll timer -------------------------------- */

static int64_t current_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t) ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

#define RECONN_CHECK_INTERVAL_MS 5000

static int poll_timer_cb(void *data)
{
    neu_plugin_t *plugin = (neu_plugin_t *) data;

    if (plugin->rk) {
        rd_kafka_poll(plugin->rk, 0);

        if (!plugin->connected) {
            int64_t now = current_time_ms();
            if (now - plugin->last_reconn_check_ms >=
                RECONN_CHECK_INTERVAL_MS) {
                plugin->last_reconn_check_ms = now;
                stats_cb_check_connected(plugin, plugin->rk);
            }
        }
    }

    return 0;
}

static int start_poll_timer(neu_plugin_t *plugin)
{
    if (NULL == plugin->events) {
        plugin->events = neu_event_new(plugin->common.name);
        if (NULL == plugin->events) {
            plog_error(plugin, "neu_event_new fail");
            return -1;
        }
    }

    neu_event_timer_param_t param = {
        .second      = 0,
        .millisecond = 200,
        .cb          = poll_timer_cb,
        .usr_data    = plugin,
    };

    neu_event_timer_t *timer = neu_event_add_timer(plugin->events, param);
    if (NULL == timer) {
        plog_error(plugin, "neu_event_add_timer fail");
        return -1;
    }

    if (plugin->poll_timer) {
        neu_event_del_timer(plugin->events, plugin->poll_timer);
    }
    plugin->poll_timer = timer;
    return 0;
}

static void stop_poll_timer(neu_plugin_t *plugin)
{
    if (plugin->poll_timer) {
        neu_event_del_timer(plugin->events, plugin->poll_timer);
        plugin->poll_timer = NULL;
    }
}

/* ----------------------------- rdkafka setup ------------------------------ */

static int set_conf(neu_plugin_t *plugin, rd_kafka_conf_t *conf,
                    const char *key, const char *val)
{
    char errstr[512];

    if (RD_KAFKA_CONF_OK !=
        rd_kafka_conf_set(conf, key, val, errstr, sizeof(errstr))) {
        plog_warn(plugin, "rd_kafka_conf_set %s=%s fail: %s", key, val, errstr);
        return -1;
    }
    return 0;
}

static void set_rdkafka_conf(neu_plugin_t *plugin, rd_kafka_conf_t *conf,
                             const kafka_config_t *config)
{
    char buf[64];

    set_conf(plugin, conf, "bootstrap.servers", config->broker);

    snprintf(buf, sizeof(buf), "%d", config->batch_max_messages);
    set_conf(plugin, conf, "queue.buffering.max.messages", buf);

    snprintf(buf, sizeof(buf), "%d", config->linger_ms);
    set_conf(plugin, conf, "linger.ms", buf);

    set_conf(plugin, conf, "compression.codec",
             kafka_compression_str(config->compression));

    snprintf(buf, sizeof(buf), "%d", config->message_timeout_ms);
    set_conf(plugin, conf, "message.timeout.ms", buf);

    snprintf(buf, sizeof(buf), "%d", config->acks);
    set_conf(plugin, conf, "acks", buf);

    if (config->client_id) {
        set_conf(plugin, conf, "client.id", config->client_id);
    }

    set_conf(plugin, conf, "security.protocol",
             kafka_security_protocol_str(config->security_protocol));

    if (config->security_protocol == KAFKA_SECURITY_SASL_PLAINTEXT ||
        config->security_protocol == KAFKA_SECURITY_SASL_SSL) {
        set_conf(plugin, conf, "sasl.mechanism",
                 kafka_sasl_mechanism_str(config->sasl_mechanism));
        if (config->sasl_username) {
            set_conf(plugin, conf, "sasl.username", config->sasl_username);
        }
        if (config->sasl_password) {
            set_conf(plugin, conf, "sasl.password", config->sasl_password);
        }
    }

    if (config->security_protocol == KAFKA_SECURITY_SSL ||
        config->security_protocol == KAFKA_SECURITY_SASL_SSL) {
        if (config->ssl_ca) {
            set_conf(plugin, conf, "ssl.ca.pem", config->ssl_ca);
        }
        if (config->ssl_cert) {
            set_conf(plugin, conf, "ssl.certificate.pem", config->ssl_cert);
        }
        if (config->ssl_key) {
            set_conf(plugin, conf, "ssl.key.pem", config->ssl_key);
        }
    }

    rd_kafka_conf_set_dr_msg_cb(conf, dr_msg_cb);
    rd_kafka_conf_set_error_cb(conf, error_cb);
    rd_kafka_conf_set_opaque(conf, plugin);
}

static rd_kafka_t *create_producer(neu_plugin_t *        plugin,
                                   const kafka_config_t *config)
{
    char             errstr[512];
    rd_kafka_conf_t *conf = rd_kafka_conf_new();

    set_rdkafka_conf(plugin, conf, config);

    rd_kafka_t *rk =
        rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr));
    if (NULL == rk) {
        plog_error(plugin, "rd_kafka_new failed: %s", errstr);
        return NULL;
    }

    return rk;
}

/* ---------------------------- plugin interface ---------------------------- */

static neu_plugin_t *kafka_plugin_open(void)
{
    neu_plugin_t *plugin = (neu_plugin_t *) calloc(1, sizeof(neu_plugin_t));
    neu_plugin_common_init(&plugin->common);
    return plugin;
}

static int kafka_plugin_close(neu_plugin_t *plugin)
{
    plog_notice(plugin, "plugin closed");
    free(plugin);
    return NEU_ERR_SUCCESS;
}

static int kafka_plugin_init(neu_plugin_t *plugin, bool load)
{
    (void) load;

    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_TRANS_DATA_5S, 5000);
    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_TRANS_DATA_30S, 30000);
    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_TRANS_DATA_60S, 60000);
    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_SEND_MSGS_TOTAL, 0);
    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_SEND_MSG_ERRORS_TOTAL, 0);
    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_SEND_BYTES_5S, 5000);
    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_SEND_BYTES_30S, 30000);
    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_SEND_BYTES_60S, 60000);
    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_DISCONNECTION_60S, 60000);
    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_DISCONNECTION_600S, 600000);
    NEU_PLUGIN_REGISTER_METRIC(plugin, NEU_METRIC_DISCONNECTION_1800S, 1800000);

    plog_notice(plugin, "plugin `%s` initialized",
                neu_plugin_module.module_name);
    return NEU_ERR_SUCCESS;
}

static int kafka_plugin_uninit(neu_plugin_t *plugin)
{
    stop_poll_timer(plugin);

    if (plugin->events) {
        neu_event_close(plugin->events);
        plugin->events = NULL;
    }

    if (plugin->rk) {
        rd_kafka_flush(plugin->rk, 5000);
        rd_kafka_destroy(plugin->rk);
        plugin->rk = NULL;
    }

    kafka_config_fini(&plugin->config);
    kafka_route_tbl_free(plugin->route_tbl);
    plugin->route_tbl = NULL;

    plog_notice(plugin, "plugin `%s` uninitialized",
                neu_plugin_module.module_name);
    return NEU_ERR_SUCCESS;
}

static int kafka_plugin_config(neu_plugin_t *plugin, const char *setting)
{
    int            rv     = 0;
    kafka_config_t config = { 0 };

    rv = kafka_config_parse(plugin, setting, &config);
    if (0 != rv) {
        plog_error(plugin, "kafka_config_parse fail");
        return NEU_ERR_NODE_SETTING_INVALID;
    }

    stop_poll_timer(plugin);

    if (plugin->rk) {
        rd_kafka_flush(plugin->rk, 3000);
        rd_kafka_destroy(plugin->rk);
        plugin->rk = NULL;
    }

    plugin->rk = create_producer(plugin, &config);
    if (NULL == plugin->rk) {
        plog_error(plugin, "create kafka producer fail");
        kafka_config_fini(&config);
        return NEU_ERR_PLUGIN_NOT_RUNNING;
    }

    if (0 != start_poll_timer(plugin)) {
        plog_error(plugin, "start poll timer fail");
        rd_kafka_destroy(plugin->rk);
        plugin->rk = NULL;
        kafka_config_fini(&config);
        return NEU_ERR_EINTERNAL;
    }

    stats_cb_check_connected(plugin, plugin->rk);

    kafka_config_fini(&plugin->config);
    plugin->config = config;

    plog_notice(plugin, "plugin `%s` configured",
                neu_plugin_module.module_name);
    return NEU_ERR_SUCCESS;
}

static int kafka_plugin_start(neu_plugin_t *plugin)
{
    if (NULL == plugin->rk) {
        plog_error(plugin, "kafka producer is NULL");
        return NEU_ERR_PLUGIN_NOT_RUNNING;
    }

    if (0 != start_poll_timer(plugin)) {
        return NEU_ERR_EINTERNAL;
    }

    stats_cb_check_connected(plugin, plugin->rk);

    plog_notice(plugin, "plugin `%s` started", neu_plugin_module.module_name);
    return NEU_ERR_SUCCESS;
}

static int kafka_plugin_stop(neu_plugin_t *plugin)
{
    stop_poll_timer(plugin);

    if (plugin->rk) {
        rd_kafka_flush(plugin->rk, 3000);
    }

    plugin->connected         = false;
    plugin->common.link_state = NEU_NODE_LINK_STATE_DISCONNECTED;

    plog_notice(plugin, "plugin `%s` stopped", neu_plugin_module.module_name);
    return NEU_ERR_SUCCESS;
}

static int kafka_plugin_request(neu_plugin_t *plugin, neu_reqresp_head_t *head,
                                void *data)
{
    neu_err_code_e error = NEU_ERR_SUCCESS;

    switch (head->type) {
    case NEU_REQRESP_TRANS_DATA:
        if (plugin->rk) {
            NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_TRANS_DATA_5S, 1, NULL);
            NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_TRANS_DATA_30S, 1,
                                     NULL);
            NEU_PLUGIN_UPDATE_METRIC(plugin, NEU_METRIC_TRANS_DATA_60S, 1,
                                     NULL);
        }
        error = handle_trans_data(plugin, data);
        break;
    case NEU_REQ_SUBSCRIBE_GROUP:
        error = handle_subscribe_group(plugin, data);
        break;
    case NEU_REQ_UPDATE_SUBSCRIBE_GROUP:
        error = handle_update_subscribe(plugin, data);
        break;
    case NEU_REQ_UNSUBSCRIBE_GROUP:
        error = handle_unsubscribe_group(plugin, data);
        break;
    case NEU_REQ_UPDATE_GROUP:
        error = handle_update_group(plugin, data);
        break;
    case NEU_REQ_DEL_GROUP:
        error = handle_del_group(plugin, data);
        break;
    case NEU_REQ_UPDATE_NODE:
        error = handle_update_driver(plugin, data);
        break;
    case NEU_REQRESP_NODE_DELETED: {
        neu_reqresp_node_deleted_t *req = data;
        if (0 != strcmp(plugin->common.name, req->node)) {
            error = handle_del_driver(plugin, data);
        }
        break;
    }
    default:
        error = NEU_ERR_PLUGIN_NOT_RUNNING;
        break;
    }

    return error;
}

const neu_plugin_intf_funs_t kafka_plugin_intf_funs = {
    .open        = kafka_plugin_open,
    .close       = kafka_plugin_close,
    .init        = kafka_plugin_init,
    .uninit      = kafka_plugin_uninit,
    .start       = kafka_plugin_start,
    .stop        = kafka_plugin_stop,
    .setting     = kafka_plugin_config,
    .request     = kafka_plugin_request,
    .try_connect = NULL,
};
