#include <gtest/gtest.h>

#include <neuron.h>

#include "mqtt_plugin.h"
#include "mqtt_util.h"

zlog_category_t *neuron = NULL;

TEST(MQTTTest, mqtt_option_init_by_config)
{
    neu_plugin_t *plugin = (neu_plugin_t *) calloc(1, sizeof(neu_plugin_t));
    neu_plugin_common_init(&plugin->common);
    neu_mqtt_option_t option;

    memset(&option, 0, sizeof(neu_mqtt_option_t));
    char *config = (char *) "{\"node_id\":5,\"params\":{}}";
    int   rc     = mqtt_option_init(plugin, config, &option);
    EXPECT_EQ(3, rc);
    mqtt_option_uninit(plugin, &option);

    memset(&option, 0, sizeof(neu_mqtt_option_t));
    config = (char *) "{\"node_id\":5,\"params\":{\"upload-topic\":\"\"}}";
    rc     = mqtt_option_init(plugin, config, &option);
    EXPECT_EQ(3, rc);
    mqtt_option_uninit(plugin, &option);

    memset(&option, 0, sizeof(neu_mqtt_option_t));
    config = (char *) "{\"node_id\":5,\"params\":{\"upload-topic\":\"/neuron/"
                      "node_name/upload\"}}";
    rc = mqtt_option_init(plugin, config, &option);
    EXPECT_EQ(4, rc);
    mqtt_option_uninit(plugin, &option);

    memset(&option, 0, sizeof(neu_mqtt_option_t));
    config = (char *) "{\"node_id\":5,\"params\":{\"upload-topic\":\"/neuron/"
                      "node_name/upload\", \"heartbeat-topic\":\"\"}}";
    rc = mqtt_option_init(plugin, config, &option);
    EXPECT_EQ(4, rc);
    mqtt_option_uninit(plugin, &option);

    memset(&option, 0, sizeof(neu_mqtt_option_t));
    config = (char *) "{\"node_id\":5,\"params\":{\"upload-topic\":\"/neuron/"
                      "node_name/upload\", "
                      "\"heartbeat-topic\":\"/neuron/node_name/heartbeat\"}}";
    rc = mqtt_option_init(plugin, config, &option);
    EXPECT_EQ(7, rc);
    mqtt_option_uninit(plugin, &option);

    memset(&option, 0, sizeof(neu_mqtt_option_t));
    config = (char *) "{\"node_id\":5,\"params\":{\"upload-topic\":\"/neuron/"
                      "node_name/upload\", "
                      "\"heartbeat-topic\":\"/neuron/node_name/heartbeat\", "
                      "\"format\":1}}";
    rc = mqtt_option_init(plugin, config, &option);
    EXPECT_EQ(7, rc);
    mqtt_option_uninit(plugin, &option);

    memset(&option, 0, sizeof(neu_mqtt_option_t));
    config = (char *) "{\"node_id\":5,\"params\":{\"upload-topic\":\"/neuron/"
                      "node_name/upload\", "
                      "\"heartbeat-topic\":\"/neuron/node_name/heartbeat\", "
                      "\"format\":1, \"ssl\":false}}";
    rc = mqtt_option_init(plugin, config, &option);
    EXPECT_EQ(7, rc);
    mqtt_option_uninit(plugin, &option);

    memset(&option, 0, sizeof(neu_mqtt_option_t));
    config = (char *) "{\"node_id\":5,\"params\":{\"upload-topic\":\"/neuron/"
                      "node_name/upload\", "
                      "\"heartbeat-topic\":\"/neuron/node_name/heartbeat\", "
                      "\"format\":1, \"ssl\":false, \"host\":\"\"}}";
    rc = mqtt_option_init(plugin, config, &option);
    EXPECT_EQ(7, rc);
    mqtt_option_uninit(plugin, &option);

    memset(&option, 0, sizeof(neu_mqtt_option_t));
    config =
        (char *) "{\"node_id\":5,\"params\":{\"upload-topic\":\"/neuron/"
                 "node_name/upload\", "
                 "\"heartbeat-topic\":\"/neuron/node_name/heartbeat\", "
                 "\"format\":1, \"ssl\":false, \"host\":\"192.168.10.116\"}}";
    rc = mqtt_option_init(plugin, config, &option);
    EXPECT_EQ(8, rc);
    mqtt_option_uninit(plugin, &option);

    memset(&option, 0, sizeof(neu_mqtt_option_t));
    config = (char *) "{\"node_id\":5,\"params\":{\"upload-topic\":\"/neuron/"
                      "node_name/upload\", "
                      "\"heartbeat-topic\":\"/neuron/node_name/heartbeat\", "
                      "\"format\":1, \"ssl\":false, "
                      "\"host\":\"192.168.10.116\", \"port\":1883}}";
    rc = mqtt_option_init(plugin, config, &option);
    EXPECT_EQ(0, rc);
    mqtt_option_uninit(plugin, &option);

    memset(&option, 0, sizeof(neu_mqtt_option_t));
    config = (char *) "{\"node_id\":5,\"params\":{\"upload-topic\":\"/neuron/"
                      "node_name/upload\", "
                      "\"heartbeat-topic\":\"/neuron/node_name/heartbeat\", "
                      "\"format\":1, \"ssl\":false, "
                      "\"host\":\"192.168.10.116\", \"port\":1883, "
                      "\"username\":\"\"}}";
    rc = mqtt_option_init(plugin, config, &option);
    EXPECT_EQ(0, rc);
    mqtt_option_uninit(plugin, &option);

    memset(&option, 0, sizeof(neu_mqtt_option_t));
    config = (char *) "{\"node_id\":5,\"params\":{\"upload-topic\":\"/neuron/"
                      "node_name/upload\", "
                      "\"heartbeat-topic\":\"/neuron/node_name/heartbeat\", "
                      "\"format\":1, \"ssl\":false, "
                      "\"host\":\"192.168.10.116\", \"port\":1883, "
                      "\"username\":\"\", \"password\":\"\"}}";
    rc = mqtt_option_init(plugin, config, &option);
    EXPECT_EQ(0, rc);
    mqtt_option_uninit(plugin, &option);

    memset(&option, 0, sizeof(neu_mqtt_option_t));
    config = (char *) "{\"node_id\":5,\"params\":{\"upload-topic\":\"/neuron/"
                      "node_name/upload\", "
                      "\"heartbeat-topic\":\"/neuron/node_name/heartbeat\", "
                      "\"format\":1, \"ssl\":true, "
                      "\"host\":\"192.168.10.116\", \"port\":1883, "
                      "\"username\":\"\", \"password\":\"\"}}";
    rc = mqtt_option_init(plugin, config, &option);
    EXPECT_EQ(5, rc);
    mqtt_option_uninit(plugin, &option);

    memset(&option, 0, sizeof(neu_mqtt_option_t));
    config = (char *) "{\"node_id\":5,\"params\":{\"upload-topic\":\"/neuron/"
                      "node_name/upload\", "
                      "\"heartbeat-topic\":\"/neuron/node_name/heartbeat\", "
                      "\"format\":1, \"ssl\":true, "
                      "\"host\":\"192.168.10.116\", \"port\":1883, "
                      "\"username\":\"\", \"password\":\"\", \"ca\":\"\"}}";
    rc = mqtt_option_init(plugin, config, &option);
    EXPECT_EQ(5, rc);
    mqtt_option_uninit(plugin, &option);

    memset(&option, 0, sizeof(neu_mqtt_option_t));
    config =
        (char *) "{\"node_id\":5,\"params\":{\"upload-topic\":\"/neuron/"
                 "node_name/upload\", "
                 "\"heartbeat-topic\":\"/neuron/node_name/heartbeat\", "
                 "\"format\":1, \"ssl\":true, "
                 "\"host\":\"192.168.10.116\", \"port\":1883, "
                 "\"username\":\"\", \"password\":\"\", \"ca\":\"ttttttttt\"}}";
    rc = mqtt_option_init(plugin, config, &option);
    EXPECT_EQ(0, rc);
    mqtt_option_uninit(plugin, &option);

    memset(&option, 0, sizeof(neu_mqtt_option_t));
    config =
        (char *) "{\"node_id\":5,\"params\":{\"upload-topic\":\"/neuron/"
                 "node_name/upload\", "
                 "\"heartbeat-topic\":\"/neuron/node_name/heartbeat\", "
                 "\"format\":1, \"ssl\":true, "
                 "\"host\":\"192.168.10.116\", \"port\":1883, "
                 "\"username\":\"\", \"password\":\"\", \"ca\":\"tttttt\", "
                 "\"cert\":\"\" }}";
    rc = mqtt_option_init(plugin, config, &option);
    EXPECT_EQ(6, rc);
    mqtt_option_uninit(plugin, &option);

    memset(&option, 0, sizeof(neu_mqtt_option_t));
    config =
        (char *) "{\"node_id\":5,\"params\":{\"upload-topic\":\"/neuron/"
                 "node_name/upload\", "
                 "\"heartbeat-topic\":\"/neuron/node_name/heartbeat\", "
                 "\"format\":1, \"ssl\":true, "
                 "\"host\":\"192.168.10.116\", \"port\":1883, "
                 "\"username\":\"\", \"password\":\"\", \"ca\":\"tttttt\", "
                 "\"cert\":\"ttttt\" }}";
    rc = mqtt_option_init(plugin, config, &option);
    EXPECT_EQ(6, rc);
    mqtt_option_uninit(plugin, &option);

    memset(&option, 0, sizeof(neu_mqtt_option_t));
    config =
        (char *) "{\"node_id\":5,\"params\":{\"upload-topic\":\"/neuron/"
                 "node_name/upload\", "
                 "\"heartbeat-topic\":\"/neuron/node_name/heartbeat\", "
                 "\"format\":1, \"ssl\":true, "
                 "\"host\":\"192.168.10.116\", \"port\":1883, "
                 "\"username\":\"\", \"password\":\"\", \"ca\":\"tttttt\", "
                 "\"cert\":\"tttttt\", \"key\":\"\" }}";
    rc = mqtt_option_init(plugin, config, &option);
    EXPECT_EQ(6, rc);
    mqtt_option_uninit(plugin, &option);

    memset(&option, 0, sizeof(neu_mqtt_option_t));
    config =
        (char *) "{\"node_id\":5,\"params\":{\"upload-topic\":\"/neuron/"
                 "node_name/upload\", "
                 "\"heartbeat-topic\":\"/neuron/node_name/heartbeat\", "
                 "\"format\":1, \"ssl\":true, "
                 "\"host\":\"192.168.10.116\", \"port\":1883, "
                 "\"username\":\"\", \"password\":\"\", \"ca\":\"tttttt\", "
                 "\"cert\":\"tttttt\", \"key\":\"tttttt\" }}";
    rc = mqtt_option_init(plugin, config, &option);
    EXPECT_EQ(0, rc);
    mqtt_option_uninit(plugin, &option);

    memset(&option, 0, sizeof(neu_mqtt_option_t));
    config =
        (char *) "{\"node_id\":5,\"params\":{\"upload-topic\":\"/neuron/"
                 "node_name/upload\", "
                 "\"heartbeat-topic\":\"/neuron/node_name/heartbeat\", "
                 "\"format\":1, \"ssl\":true, "
                 "\"host\":\"192.168.10.116\", \"port\":1883, "
                 "\"username\":\"\", \"password\":\"\", \"ca\":\"tttttt\", "
                 "\"cert\":\"ttttt\", \"key\":\"tttt\", \"keypass\":\"\" }}";
    rc = mqtt_option_init(plugin, config, &option);
    EXPECT_EQ(0, rc);
    mqtt_option_uninit(plugin, &option);

    free(plugin);
}

int main(int argc, char **argv)
{
    zlog_init("./config/dev.conf");
    neuron = zlog_get_category("neuron");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
