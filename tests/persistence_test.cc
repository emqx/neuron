#include <gtest/gtest.h>

#include "persist/persist.h"

TEST(PersistenceAdapterTest, Adapter)
{
    const char *     dir_name    = strdup("./persistence");
    neu_persister_t *persister   = neu_persister_create(dir_name);
    const char *     persist_dir = neu_persister_get_persist_dir(persister);
    const char *adapters_fname   = neu_persister_get_adapters_fname(persister);
    const char *plugins_fname    = neu_persister_get_plugins_fname(persister);

    neu_json_node_req_node_t node1 = { 1, (char *) "sample-driver-adapter", 1,
                                       (char *) "sample-drv-plugin", 1 };
    neu_json_node_req_node_t node2 = { 2, (char *) "sample-app-adapter", 5,
                                       (char *) "sample-app-plugin", 2 };
    neu_json_node_req_node_t node3 = { 3, (char *) "mqtt-adapter", 3,
                                       (char *) "mqtt-plugin", 3 };

    neu_json_node_req_t *req =
        (neu_json_node_req_t *) malloc(sizeof(neu_json_node_req_t));
    req->nodes = (neu_json_node_req_node_t *) calloc(
        3, sizeof(neu_json_node_req_node_t));

    req->n_node   = 3;
    req->nodes[0] = node1;
    req->nodes[1] = node2;
    req->nodes[2] = node3;

    vector_t *vec = vector_new_move_from_buf(
        req->nodes, req->n_node, req->n_node, sizeof(neu_json_node_req_node_t));

    EXPECT_EQ(0, neu_persister_store_adapters(persister, vec));

    vector_t *nodes = NULL;
    EXPECT_EQ(0, neu_persister_load_adapters(persister, &nodes));

    VECTOR_FOR_EACH(nodes, iter)
    {
        neu_json_node_req_node_t *info =
            (neu_json_node_req_node_t *) iterator_get(&iter);

        if (strcmp(info->name, "sample-driver-adapter") == 0) {
            EXPECT_EQ(1, info->type);
            EXPECT_STREQ("sample-drv-plugin", info->plugin_name);
            EXPECT_EQ(1, info->state);
        } else if (strcmp(info->name, "sample-app-adapter") == 0) {
            EXPECT_EQ(5, info->type);
            EXPECT_STREQ("sample-app-plugin", info->plugin_name);
            EXPECT_EQ(2, info->state);
        } else if (strcmp(info->name, "mqtt-adapter") == 0) {
            EXPECT_EQ(3, info->type);
            EXPECT_STREQ("mqtt-plugin", info->plugin_name);
            EXPECT_EQ(3, info->state);
        } else {
            EXPECT_EQ(1, 2);
        }

        free(info->name);
        free(info->plugin_name);
    }

    vector_free(nodes);

    vector_free(vec);
    free((char *) dir_name);

    free((char *) persist_dir);
    free((char *) adapters_fname);
    free((char *) plugins_fname);
    free(persister);

    free(req);
}

TEST(PersistencePluginTest, Plugin)
{
    const char *     dir_name    = strdup("./persistence");
    neu_persister_t *persister   = neu_persister_create(dir_name);
    const char *     persist_dir = neu_persister_get_persist_dir(persister);
    const char *adapters_fname   = neu_persister_get_adapters_fname(persister);
    const char *plugins_fname    = neu_persister_get_plugins_fname(persister);

    neu_json_plugin_req_plugin_t plugin1 = { (
        char *) "libplugin-sample-drv.so" };
    neu_json_plugin_req_plugin_t plugin2 = { (
        char *) "libplugin-sample-app.so" };
    neu_json_plugin_req_plugin_t plugin3 = { (char *) "libplugin-mqtt.so" };

    neu_json_plugin_req_t *req =
        (neu_json_plugin_req_t *) malloc(sizeof(neu_json_plugin_req_t));
    req->plugins = (neu_json_plugin_req_plugin_t *) calloc(
        3, sizeof(neu_json_plugin_req_plugin_t));
    req->n_plugin   = 3;
    req->plugins[0] = plugin1;
    req->plugins[1] = plugin2;
    req->plugins[2] = plugin3;

    vector_t *vec =
        vector_new_move_from_buf(req->plugins, req->n_plugin, req->n_plugin,
                                 sizeof(neu_json_plugin_req_plugin_t));

    EXPECT_EQ(0, neu_persister_store_plugins(persister, vec));

    vector_t *plugins = NULL;
    EXPECT_EQ(0, neu_persister_load_plugins(persister, &plugins));
    VECTOR_FOR_EACH(plugins, iter)
    {
        neu_json_plugin_req_plugin_t *info =
            (neu_json_plugin_req_plugin_t *) iterator_get(&iter);

        if (strcmp(*info, "libplugin-sample-drv.so") == 0) {
            EXPECT_STREQ("libplugin-sample-drv.so", *info);
        } else if (strcmp(*info, "libplugin-sample-app.so") == 0) {
            EXPECT_STREQ("libplugin-sample-app.so", *info);
        } else if (strcmp(*info, "libplugin-mqtt.so") == 0) {
            EXPECT_STREQ("libplugin-mqtt.so", *info);
        } else {
            EXPECT_EQ(1, 2);
        }
        free(*info);
    }

    vector_free(plugins);
    vector_free(vec);

    free(req);
    free((char *) persist_dir);
    free((char *) adapters_fname);
    free((char *) plugins_fname);
    free(persister);
    free((char *) dir_name);
}

TEST(PersistenceDatatagTest, Datatag)
{
    const char *     dir_name    = strdup("./persistence");
    neu_persister_t *persister   = neu_persister_create(dir_name);
    const char *     persist_dir = neu_persister_get_persist_dir(persister);
    const char *adapters_fname   = neu_persister_get_adapters_fname(persister);
    const char *grp_config_name  = "test-group";
    const char *plugins_fname    = neu_persister_get_plugins_fname(persister);

    neu_json_datatag_req_tag_t tag1 = { 1, (char *) "1!400001",
                                        (char *) "modbus1", 3, 1 };
    neu_json_datatag_req_tag_t tag2 = { 2, (char *) "1!400002",
                                        (char *) "modbus2", 4, 1 };
    neu_json_datatag_req_tag_t tag3 = { 3, (char *) "1!400003",
                                        (char *) "modbus3", 5, 1 };

    neu_json_datatag_req_t *req =
        (neu_json_datatag_req_t *) malloc(sizeof(neu_json_datatag_req_t));
    req->tags = (neu_json_datatag_req_tag_t *) calloc(
        3, sizeof(neu_json_datatag_req_tag_t));
    req->n_tag   = 3;
    req->tags[0] = tag1;
    req->tags[1] = tag2;
    req->tags[2] = tag3;

    vector_t *vec = vector_new_move_from_buf(
        req->tags, req->n_tag, req->n_tag, sizeof(neu_json_datatag_req_tag_t));

    char *adapter_name = strdup("adapter_modbus");
    EXPECT_EQ(0,
              neu_persister_store_datatags(persister, adapter_name,
                                           grp_config_name, vec));

    vector_t *tags = NULL;
    EXPECT_EQ(0,
              neu_persister_load_datatags(persister, adapter_name,
                                          grp_config_name, &tags));
    VECTOR_FOR_EACH(tags, iter)
    {
        neu_json_datatag_req_tag_t *info =
            (neu_json_datatag_req_tag_t *) iterator_get(&iter);

        if (strcmp(info->name, "modbus1") == 0) {
            EXPECT_STREQ("1!400001", info->address);
            EXPECT_EQ(3, info->type);
            EXPECT_EQ(1, info->attribute);
            EXPECT_EQ(1, info->id);
        } else if (strcmp(info->name, "modbus2") == 0) {
            EXPECT_STREQ("1!400002", info->address);
            EXPECT_EQ(4, info->type);
            EXPECT_EQ(1, info->attribute);
            EXPECT_EQ(2, info->id);
        } else if (strcmp(info->name, "modbus3") == 0) {
            EXPECT_STREQ("1!400003", info->address);
            EXPECT_EQ(5, info->type);
            EXPECT_EQ(1, info->attribute);
            EXPECT_EQ(3, info->id);
        } else {
            EXPECT_EQ(1, 2);
        }

        free(info->name);
        free(info->address);
    }

    vector_free(tags);
    free(adapter_name);

    vector_free(vec);
    free(req);

    free((char *) persist_dir);
    free((char *) adapters_fname);
    free((char *) plugins_fname);
    free(persister);

    free((char *) dir_name);
}

TEST(PersistenceSubscriptionTest, Subscription)
{
    const char *     dir_name    = strdup("./persistence");
    neu_persister_t *persister   = neu_persister_create(dir_name);
    const char *     persist_dir = neu_persister_get_persist_dir(persister);
    const char *adapters_fname   = neu_persister_get_adapters_fname(persister);
    const char *grp_config_name  = "test-group";
    const char *plugins_fname    = neu_persister_get_plugins_fname(persister);

    neu_json_subscriptions_req_subscription_t sub1 = {
        (char *) "config_drv_sample", (char *) "sample-app-adapter", 2000,
        (char *) "sample-driver-adapter"
    };
    neu_json_subscriptions_req_subscription_t sub2 = {
        (char *) "config_app_sample", (char *) "mqtt-adapter", 2000,
        (char *) "config_app_sample"
    };
    neu_json_subscriptions_req_subscription_t sub3 = {
        (char *) "config_modbus_tcp_sample_2", (char *) "mqtt-adapter", 2000,
        (char *) "modbus_tcp_adapter"
    };

    neu_json_subscriptions_req_t *req = (neu_json_subscriptions_req_t *) malloc(
        sizeof(neu_json_subscriptions_req_t));
    req->subscriptions = (neu_json_subscriptions_req_subscription_t *) calloc(
        3, sizeof(neu_json_subscriptions_req_subscription_t));
    req->n_subscription   = 3;
    req->subscriptions[0] = sub1;
    req->subscriptions[1] = sub2;
    req->subscriptions[2] = sub3;

    char *    adapter_name = strdup("adapter_mqtt");
    vector_t *vec          = vector_new_move_from_buf(
        req->subscriptions, req->n_subscription, req->n_subscription,
        sizeof(neu_json_subscriptions_req_subscription_t));

    EXPECT_EQ(0,
              neu_persister_store_subscriptions(persister, adapter_name, vec));

    vector_t *subs = NULL;
    EXPECT_EQ(0,
              neu_persister_load_subscriptions(persister, adapter_name, &subs));
    VECTOR_FOR_EACH(subs, iter)
    {
        neu_json_subscriptions_req_subscription_t *info =
            (neu_json_subscriptions_req_subscription_t *) iterator_get(&iter);

        if (strcmp(info->group_config_name, "config_drv_sample") == 0) {
            EXPECT_STREQ("sample-app-adapter", info->sub_adapter_name);
            EXPECT_EQ(2000, info->read_interval);
            EXPECT_STREQ("sample-driver-adapter", info->src_adapter_name);
        } else if (strcmp(info->group_config_name, "config_app_sample") == 0) {
            EXPECT_STREQ("mqtt-adapter", info->sub_adapter_name);
            EXPECT_EQ(2000, info->read_interval);
            EXPECT_STREQ("config_app_sample", info->src_adapter_name);
        } else if (strcmp(info->group_config_name,
                          "config_modbus_tcp_sample_2") == 0) {
            EXPECT_STREQ("mqtt-adapter", info->sub_adapter_name);
            EXPECT_EQ(2000, info->read_interval);
            EXPECT_STREQ("modbus_tcp_adapter", info->src_adapter_name);
        } else {
            EXPECT_EQ(1, 2);
        }

        free(info->group_config_name);
        free(info->sub_adapter_name);
        free(info->src_adapter_name);
    }

    vector_free(subs);
    free((char *) dir_name);

    free((char *) persist_dir);
    free((char *) adapters_fname);
    free((char *) plugins_fname);
    free(persister);

    free(req);
    free(adapter_name);
    vector_free(vec);
}
