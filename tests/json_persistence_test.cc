#include <stdlib.h>

#include <gtest/gtest.h>

#include "persist/json/persist_json_adapter.h"
#include "persist/json/persist_json_datatags.h"
#include "persist/json/persist_json_group_configs.h"
#include "persist/json/persist_json_plugin.h"
#include "persist/json/persist_json_subs.h"
#include "json/neu_json_fn.h"

TEST(JsonAdapter, AdapterPersistenceDecode)
{
    char *buf =
        (char *) "{\"nodes\": "
                 "[{\"id\":1, \"type\":1, \"state\": 1, \"name\": "
                 "\"sample-driver-adapter\", \"plugin_name\": "
                 "\"sample-drv-plugin\"}, "
                 "{\"id\":2, \"type\":5, \"state\": 2, \"name\": "
                 "\"sample-app-adapter\", \"plugin_name\": "
                 "\"sample-app-plugin\"}, "
                 "{\"id\":3, \"type\": 3, \"state\": 3, \"name\": "
                 "\"mqtt-adapter\", \"plugin_name\": \"mqtt-plugin\"}]}";

    neu_json_node_req_t *req = NULL;

    EXPECT_EQ(0, neu_json_decode_node_req(buf, &req));

    EXPECT_EQ(1, req->nodes[0].state);
    EXPECT_EQ(1, req->nodes[0].type);
    EXPECT_STREQ("sample-driver-adapter", req->nodes[0].name);
    EXPECT_STREQ("sample-drv-plugin", req->nodes[0].plugin_name);

    EXPECT_EQ(2, req->nodes[1].state);
    EXPECT_EQ(5, req->nodes[1].type);
    EXPECT_STREQ("sample-app-adapter", req->nodes[1].name);
    EXPECT_STREQ("sample-app-plugin", req->nodes[1].plugin_name);

    EXPECT_EQ(3, req->nodes[2].state);
    EXPECT_EQ(3, req->nodes[2].type);
    EXPECT_STREQ("mqtt-adapter", req->nodes[2].name);
    EXPECT_STREQ("mqtt-plugin", req->nodes[2].plugin_name);

    neu_json_decode_node_req_free(req);
}

TEST(JsonPlugin, PluginPersistenceDecode)
{
    char *buf =
        (char *) "{\"plugins\": "
                 "[ \"libplugin-sample-drv.so\", \"libplugin-sample-app.so\", "
                 "\"libplugin-mqtt.so\" ]}";

    neu_json_plugin_req_t *req = NULL;

    EXPECT_EQ(0, neu_json_decode_plugin_req(buf, &req));

    EXPECT_STREQ("libplugin-sample-drv.so", req->plugins[0]);
    EXPECT_STREQ("libplugin-sample-app.so", req->plugins[1]);
    EXPECT_STREQ("libplugin-mqtt.so", req->plugins[2]);

    neu_json_decode_plugin_req_free(req);
}

TEST(JsonDatatags, DatatagPersistenceDecode)
{
    char *buf =
        (char *) "{\"tags\": [{\"id\": 1, \"attribute\": 1, \"type\": 3, "
                 "\"address\": \"1!400001\", \"name\": \"modbus1\"}, "
                 "{\"id\": 2, \"attribute\": 1, \"type\": 4, \"address\": "
                 "\"1!400002\", "
                 "\"name\": \"modbus2\"}, {\"id\": 3, \"attribute\": 1, "
                 "\"type\": 4, \"address\": \"1!400003\", \"name\": "
                 "\"modbus3\"}]}";

    neu_json_datatag_req_t *req = NULL;

    EXPECT_EQ(0, neu_json_decode_datatag_req(buf, &req));

    EXPECT_EQ(1, req->tags[0].id);
    EXPECT_EQ(1, req->tags[0].attribute);
    EXPECT_EQ(3, req->tags[0].type);
    EXPECT_STREQ("1!400001", req->tags[0].address);
    EXPECT_STREQ("modbus1", req->tags[0].name);

    EXPECT_EQ(2, req->tags[1].id);
    EXPECT_EQ(1, req->tags[1].attribute);
    EXPECT_EQ(4, req->tags[1].type);
    EXPECT_STREQ("1!400002", req->tags[1].address);
    EXPECT_STREQ("modbus2", req->tags[1].name);

    EXPECT_EQ(3, req->tags[2].id);
    EXPECT_EQ(1, req->tags[2].attribute);
    EXPECT_EQ(4, req->tags[2].type);
    EXPECT_STREQ("1!400003", req->tags[2].address);
    EXPECT_STREQ("modbus3", req->tags[2].name);

    neu_json_decode_datatag_req_free(req);
}

TEST(JsonSubs, SubsPersistenceDecode)
{
    char *buf = (char *) "{\"subscriptions\": [{\"sub_adapter_name\": "
                         "\"sample-app-adapter\", \"group_config_name\": "
                         "\"config_drv_sample\", \"src_adapter_name\": "
                         "\"sample-driver-adapter\", \"read_interval\": 2000}, "
                         "{\"sub_adapter_name\": \"sample-driver-adapter\", "
                         "\"group_config_name\": \"config_app_sample\", "
                         "\"src_adapter_name\": \"config_app_sample\", "
                         "\"read_interval\": 2000}, "
                         "{\"sub_adapter_name\": \"mqtt-adapter\", "
                         "\"group_config_name\": \"config_mqtt_sample\", "
                         "\"src_adapter_name\": \"sample-driver-adapter\", "
                         "\"read_interval\": 2000}]}";

    neu_json_subscriptions_req_t *req = NULL;

    EXPECT_EQ(0, neu_json_decode_subscriptions_req(buf, &req));

    EXPECT_STREQ("sample-app-adapter", req->subscriptions[0].sub_adapter_name);
    EXPECT_STREQ("config_drv_sample", req->subscriptions[0].group_config_name);
    EXPECT_STREQ("sample-driver-adapter",
                 req->subscriptions[0].src_adapter_name);
    EXPECT_EQ(2000, req->subscriptions[0].read_interval);

    EXPECT_STREQ("sample-driver-adapter",
                 req->subscriptions[1].sub_adapter_name);
    EXPECT_STREQ("config_app_sample", req->subscriptions[1].group_config_name);
    EXPECT_STREQ("config_app_sample", req->subscriptions[1].src_adapter_name);
    EXPECT_EQ(2000, req->subscriptions[1].read_interval);

    EXPECT_STREQ("mqtt-adapter", req->subscriptions[2].sub_adapter_name);
    EXPECT_STREQ("config_mqtt_sample", req->subscriptions[2].group_config_name);
    EXPECT_STREQ("sample-driver-adapter",
                 req->subscriptions[2].src_adapter_name);
    EXPECT_EQ(2000, req->subscriptions[2].read_interval);

    neu_json_decode_subscriptions_req_free(req);
}

TEST(JsonGroupConfigs, GroupConfigsPersistenceDecode)
{
    char *buf =
        (char *) "{\"adapter_name\": \"modbus_tcp_adapter\", "
                 "\"group_config_name\": \"config_modbus_tcp_sample_2\", "
                 "\"read_interval\": 2000 }";

    neu_json_group_configs_req_t *req = NULL;

    EXPECT_EQ(0, neu_json_decode_group_configs_req(buf, &req));

    EXPECT_STREQ("modbus_tcp_adapter", req->adapter_name);
    EXPECT_STREQ("config_modbus_tcp_sample_2", req->group_config_name);
    EXPECT_EQ(2000, req->read_interval);

    neu_json_decode_group_configs_req_free(req);
}

TEST(JsonAdapterTest, AdapterPersistenceEncode)
{
    char *buf =
        (char *) "{\"nodes\": [{\"type\": 1, \"state\": 1, \"plugin_name\": "
                 "\"sample-drv-plugin\", \"name\": \"sample-driver-adapter\", "
                 "\"id\": 1}, {\"type\": 5, \"state\": 2, \"plugin_name\": "
                 "\"sample-app-plugin\", \"name\": \"sample-app-adapter\", "
                 "\"id\": 2}, {\"type\": 3, \"state\": 3, \"plugin_name\": "
                 "\"mqtt-plugin\", \"name\": \"mqtt-adapter\", \"id\": 3}]}";
    char *buf2 = (char *) "{\"nodes\": []}";

    char *result  = NULL;
    char *result2 = NULL;

    neu_json_node_resp_t resp = {
        .n_node = 3,
    };
    neu_json_node_resp_t resp2 = {
        .n_node = 0,
    };

    resp.nodes = (neu_json_node_resp_node_t *) calloc(
        3, sizeof(neu_json_node_resp_node_t));
    resp.nodes[0].id          = 1;
    resp.nodes[0].state       = 1;
    resp.nodes[0].type        = 1;
    resp.nodes[0].name        = strdup((char *) "sample-driver-adapter");
    resp.nodes[0].plugin_name = strdup((char *) "sample-drv-plugin");

    resp.nodes[1].id          = 2;
    resp.nodes[1].state       = 2;
    resp.nodes[1].type        = 5;
    resp.nodes[1].name        = strdup((char *) "sample-app-adapter");
    resp.nodes[1].plugin_name = strdup((char *) "sample-app-plugin");

    resp.nodes[2].id          = 3;
    resp.nodes[2].state       = 3;
    resp.nodes[2].type        = 3;
    resp.nodes[2].name        = strdup((char *) "mqtt-adapter");
    resp.nodes[2].plugin_name = strdup((char *) "mqtt-plugin");

    EXPECT_EQ(0,
              neu_json_encode_by_fn(&resp, neu_json_encode_node_resp, &result));
    EXPECT_EQ(
        0, neu_json_encode_by_fn(&resp2, neu_json_encode_node_resp, &result2));
    EXPECT_STREQ(buf, result);
    EXPECT_STREQ(buf2, result2);

    free(resp.nodes[0].name);
    free(resp.nodes[0].plugin_name);
    free(resp.nodes[1].name);
    free(resp.nodes[1].plugin_name);
    free(resp.nodes[2].name);
    free(resp.nodes[2].plugin_name);
    free(resp.nodes);

    free(result);
    free(result2);
}

TEST(JsonPluginTest, PluginPersistenceEncode)
{
    char *buf = (char *) "{\"plugins\": [\"libplugin-sample-drv.so\", "
                         "\"libplugin-sample-app.so\", \"libplugin-mqtt.so\"]}";
    char *buf2 = (char *) "{\"plugins\": []}";

    char *result  = NULL;
    char *result2 = NULL;

    neu_json_plugin_resp_t resp = {
        .n_plugin = 3,
    };
    neu_json_plugin_resp_t resp2 = {
        .n_plugin = 0,
    };

    resp.plugins = (neu_json_plugin_resp_plugin_t *) calloc(
        3, sizeof(neu_json_plugin_resp_plugin_t));

    resp.plugins[0] = strdup("libplugin-sample-drv.so");
    resp.plugins[1] = strdup("libplugin-sample-app.so");
    resp.plugins[2] = strdup("libplugin-mqtt.so");

    EXPECT_EQ(
        0, neu_json_encode_by_fn(&resp, neu_json_encode_plugin_resp, &result));
    EXPECT_EQ(
        0,
        neu_json_encode_by_fn(&resp2, neu_json_encode_plugin_resp, &result2));

    EXPECT_STREQ(buf, result);
    EXPECT_STREQ(buf2, result2);

    free(resp.plugins[0]);
    free(resp.plugins[1]);
    free(resp.plugins[2]);
    free(resp.plugins);

    free(result);
    free(result2);
}

TEST(JsonSubscriptionsTest, SbuscriptionsPersistenceEncode)
{
    char *buf = (char *) "{\"subscriptions\": [{\"sub_adapter_name\": "
                         "\"sub_adapter_name\", \"src_adapter_name\": "
                         "\"sample-driver-adapter\", \"read_interval\": 2000, "
                         "\"group_config_name\": \"config_drv_sample\"}, "
                         "{\"sub_adapter_name\": \"sample-driver-adapter\", "
                         "\"src_adapter_name\": \"config_app_sample\", "
                         "\"read_interval\": 2000, \"group_config_name\": "
                         "\"config_app_sample\"}, {\"sub_adapter_name\": "
                         "\"mqtt-adapter\", \"src_adapter_name\": "
                         "\"sample-driver-adapter\", \"read_interval\": 2000, "
                         "\"group_config_name\": \"config_mqtt_sample\"}]}";
    char *buf2 = (char *) "{\"subscriptions\": []}";

    char *result  = NULL;
    char *result2 = NULL;

    neu_json_subscriptions_resp_t resp = {
        .n_subscription = 3,
    };
    neu_json_subscriptions_resp_t resp2 = {
        .n_subscription = 0,
    };

    resp.subscriptions = (neu_json_subscriptions_resp_subscription_t *) calloc(
        3, sizeof(neu_json_subscriptions_resp_subscription_t));
    resp.subscriptions[0].group_config_name = strdup("config_drv_sample");
    resp.subscriptions[0].sub_adapter_name  = strdup("sub_adapter_name");
    resp.subscriptions[0].src_adapter_name  = strdup("sample-driver-adapter");
    resp.subscriptions[0].read_interval     = 2000;

    resp.subscriptions[1].group_config_name = strdup("config_app_sample");
    resp.subscriptions[1].sub_adapter_name  = strdup("sample-driver-adapter");
    resp.subscriptions[1].src_adapter_name  = strdup("config_app_sample");
    resp.subscriptions[1].read_interval     = 2000;

    resp.subscriptions[2].group_config_name = strdup("config_mqtt_sample");
    resp.subscriptions[2].sub_adapter_name  = strdup("mqtt-adapter");
    resp.subscriptions[2].src_adapter_name  = strdup("sample-driver-adapter");
    resp.subscriptions[2].read_interval     = 2000;

    EXPECT_EQ(0,
              neu_json_encode_by_fn(&resp, neu_json_encode_subscriptions_resp,
                                    &result));
    EXPECT_EQ(0,
              neu_json_encode_by_fn(&resp2, neu_json_encode_subscriptions_resp,
                                    &result2));

    EXPECT_STREQ(buf, result);
    EXPECT_STREQ(buf2, result2);

    free(resp.subscriptions[0].group_config_name);
    free(resp.subscriptions[0].src_adapter_name);
    free(resp.subscriptions[0].sub_adapter_name);
    free(resp.subscriptions[1].group_config_name);
    free(resp.subscriptions[1].src_adapter_name);
    free(resp.subscriptions[1].sub_adapter_name);
    free(resp.subscriptions[2].group_config_name);
    free(resp.subscriptions[2].src_adapter_name);
    free(resp.subscriptions[2].sub_adapter_name);
    free(resp.subscriptions);

    free(result);
    free(result2);
}

TEST(JsonDatatags, DatatagPersistenceEncode)
{
    char *buf =
        (char *) "{\"tags\": [{\"type\": 3, \"name\": \"modbus1\", "
                 "\"attribute\": 1, \"address\": \"1!400001\", \"id\": 1}, {"
                 "\"type\": 4, "
                 "\"name\": \"modbus2\", \"attribute\": 1, \"address\": "
                 "\"1!400002\", \"id\": 2}, {\"type\": 4, \"name\": "
                 "\"modbus3\", "
                 "\"attribute\": 1, \"address\": \"1!400003\", \"id\": 3}]}";
    char *buf2 = (char *) "{\"tags\": []}";

    char *result  = NULL;
    char *result2 = NULL;

    neu_json_datatag_resp_t resp = {
        .n_tag = 3,
    };
    neu_json_datatag_resp_t resp2 = {
        .n_tag = 0,
    };

    resp.tags = (neu_json_datatag_resp_tag_t *) calloc(
        3, sizeof(neu_json_datatag_resp_tag_t));
    resp.tags[0].attribute = 1;
    resp.tags[0].type      = 3;
    resp.tags[0].address   = strdup("1!400001");
    resp.tags[0].name      = strdup("modbus1");
    resp.tags[0].id        = 1;

    resp.tags[1].attribute = 1;
    resp.tags[1].type      = 4;
    resp.tags[1].address   = strdup("1!400002");
    resp.tags[1].name      = strdup("modbus2");
    resp.tags[1].id        = 2;

    resp.tags[2].attribute = 1;
    resp.tags[2].type      = 4;
    resp.tags[2].address   = strdup("1!400003");
    resp.tags[2].name      = strdup("modbus3");
    resp.tags[2].id        = 3;

    EXPECT_EQ(
        0, neu_json_encode_by_fn(&resp, neu_json_encode_datatag_resp, &result));
    EXPECT_EQ(
        0,
        neu_json_encode_by_fn(&resp2, neu_json_encode_datatag_resp, &result2));

    EXPECT_STREQ(buf, result);
    EXPECT_STREQ(buf2, result2);

    free(resp.tags[0].address);
    free(resp.tags[0].name);
    free(resp.tags[1].address);
    free(resp.tags[1].name);
    free(resp.tags[2].address);
    free(resp.tags[2].name);
    free(resp.tags);

    free(result);
    free(result2);
}

TEST(JsonGroupConfigTest, GroupConfigPersistenceEncode)
{
    char *buf = (char *) "{\"read_interval\": 2000, \"group_config_name\": "
                         "\"config_modbus_tcp_sample_2\", \"adapter_name\": "
                         "\"config_modbus_tcp_sample_2\"}";

    char *buf2 = (char *) "{\"read_interval\": 2000, \"group_config_name\": "
                          "\"\", \"adapter_name\": \"\"}";

    char *result  = NULL;
    char *result2 = NULL;

    neu_json_group_configs_resp_t resp = {
        .group_config_name = strdup("config_modbus_tcp_sample_2"),
        .adapter_name      = strdup("config_modbus_tcp_sample_2"),
        .read_interval     = 2000,
    };
    neu_json_group_configs_resp_t resp2 = {
        .group_config_name = strdup(""),
        .adapter_name      = strdup(""),
        .read_interval     = 2000,
    };

    EXPECT_EQ(0,
              neu_json_encode_by_fn(&resp, neu_json_encode_group_configs_resp,
                                    &result));
    EXPECT_EQ(0,
              neu_json_encode_by_fn(&resp2, neu_json_encode_group_configs_resp,
                                    &result2));

    EXPECT_STREQ(buf, result);
    EXPECT_STREQ(buf2, result2);

    free(resp.adapter_name);
    free(resp.group_config_name);

    free(resp2.adapter_name);
    free(resp2.group_config_name);

    free(result);
    free(result2);
}
