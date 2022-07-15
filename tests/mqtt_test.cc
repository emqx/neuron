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
    // memset(&option, 0, sizeof(neu_mqtt_option_t));
    // char *config =
    //     (char *) "{\"node_id\":5,\"params\":{\"client-id\":"
    //              "\"upload123\", \"upload-topic\":\"\", "
    //              "\"heartbeat-topic\":\"\", \"format\": "
    //              "0,\"ssl\":false,\"host\":\"192.168.10."
    //              "116\",\"port\":1883,\"username\":\"\",\"password\":"
    //              "\"\",\"ca\":\"\", \"cert\":\"\", \"key\":\"\", "
    //              "\"keypass\":\"\"}}";

    // int rc = mqtt_option_init(plugin, config, &option);
    // EXPECT_EQ(0, rc);
    // mqtt_option_uninit(plugin, &option);

    memset(&option, 0, sizeof(neu_mqtt_option_t));
    char *config0 = (char *) "{\"node_id\":5,\"params\":{}}";
    int   rc      = mqtt_option_init(plugin, config0, &option);
    EXPECT_EQ(-1, rc);
    mqtt_option_uninit(plugin, &option);

    memset(&option, 0, sizeof(neu_mqtt_option_t));
    char *config1 =
        (char *) "{\"node_id\":5,\"params\":{\"client-id\":\"upload123\"}}";
    rc = mqtt_option_init(plugin, config1, &option);
    EXPECT_EQ(-2, rc);
    mqtt_option_uninit(plugin, &option);

    memset(&option, 0, sizeof(neu_mqtt_option_t));
    char *config2 = (char *) "{\"node_id\":5,\"params\":{\"client-id\":"
                             "\"upload123\", \"upload-topic\":\"\"}}";
    rc = mqtt_option_init(plugin, config2, &option);
    EXPECT_EQ(-2, rc);
    mqtt_option_uninit(plugin, &option);

    memset(&option, 0, sizeof(neu_mqtt_option_t));
    char *config3 =
        (char *) "{\"node_id\":5,\"params\":{\"client-id\":\"upload123\", "
                 "\"upload-topic\":\"\", \"heartbeat-topic\":\"\"}}";
    rc = mqtt_option_init(plugin, config3, &option);
    EXPECT_EQ(-2, rc);
    mqtt_option_uninit(plugin, &option);

    memset(&option, 0, sizeof(neu_mqtt_option_t));
    char *config4 = (char *) "{\"node_id\":5,\"params\":{\"client-id\":"
                             "\"upload123\", \"upload-topic\":\"\", "
                             "\"heartbeat-topic\":\"\", \"format\": 0}}";
    rc = mqtt_option_init(plugin, config4, &option);
    EXPECT_EQ(-2, rc);
    mqtt_option_uninit(plugin, &option);

    memset(&option, 0, sizeof(neu_mqtt_option_t));
    char *config5 =
        (char *) "{\"node_id\":5,\"params\":{\"client-id\":\"upload123\", "
                 "\"upload-topic\":\"\", \"heartbeat-topic\":\"\", \"format\": "
                 "0,\"ssl\":false}}";
    rc = mqtt_option_init(plugin, config5, &option);
    EXPECT_EQ(-5, rc);
    mqtt_option_uninit(plugin, &option);

    memset(&option, 0, sizeof(neu_mqtt_option_t));
    char *config6 =
        (char *) "{\"node_id\":5,\"params\":{\"client-id\":\"upload123\", "
                 "\"upload-topic\":\"\", \"heartbeat-topic\":\"\", \"format\": "
                 "0,\"ssl\":false,\"host\":\"192.168.10.116\"}}";
    rc = mqtt_option_init(plugin, config6, &option);
    EXPECT_EQ(-6, rc);
    mqtt_option_uninit(plugin, &option);

    memset(&option, 0, sizeof(neu_mqtt_option_t));
    char *config7 =
        (char *) "{\"node_id\":5,\"params\":{\"client-id\":\"upload123\", "
                 "\"upload-topic\":\"\", \"heartbeat-topic\":\"\", \"format\": "
                 "0,\"ssl\":false,\"host\":\"192.168.10.116\",\"port\":1883}}";
    rc = mqtt_option_init(plugin, config7, &option);
    EXPECT_EQ(0, rc);
    mqtt_option_uninit(plugin, &option);

    memset(&option, 0, sizeof(neu_mqtt_option_t));
    char *config8 =
        (char *) "{\"node_id\":5,\"params\":{\"client-id\":\"upload123\", "
                 "\"upload-topic\":\"\", \"heartbeat-topic\":\"\", \"format\": "
                 "0,\"ssl\":false,\"host\":\"192.168.10.116\",\"port\":1883,"
                 "\"username\":\"\"}}";
    rc = mqtt_option_init(plugin, config8, &option);
    EXPECT_EQ(0, rc);
    mqtt_option_uninit(plugin, &option);

    memset(&option, 0, sizeof(neu_mqtt_option_t));
    char *config9 =
        (char *) "{\"node_id\":5,\"params\":{\"client-id\":\"upload123\", "
                 "\"upload-topic\":\"\", \"heartbeat-topic\":\"\", \"format\": "
                 "0,\"ssl\":false,\"host\":\"192.168.10.116\",\"port\":1883,"
                 "\"username\":\"\",\"password\":\"\"}}";
    rc = mqtt_option_init(plugin, config9, &option);
    EXPECT_EQ(0, rc);
    mqtt_option_uninit(plugin, &option);

    memset(&option, 0, sizeof(neu_mqtt_option_t));
    char *config10 =
        (char *) "{\"node_id\":5,\"params\":{\"client-id\":\"upload123\", "
                 "\"upload-topic\":\"\", \"heartbeat-topic\":\"\", \"format\": "
                 "0,\"ssl\":true,\"host\":\"192.168.10.116\",\"port\":1883,"
                 "\"username\":\"\",\"password\":\"\"}}";
    rc = mqtt_option_init(plugin, config10, &option);
    EXPECT_EQ(-3, rc);
    mqtt_option_uninit(plugin, &option);

    memset(&option, 0, sizeof(neu_mqtt_option_t));
    char *config11 =
        (char *) "{\"node_id\":5,\"params\":{\"client-id\":\"upload123\", "
                 "\"upload-topic\":\"\", \"heartbeat-topic\":\"\", \"format\": "
                 "0,\"ssl\":true,\"host\":\"192.168.10.116\",\"port\":1883,"
                 "\"username\":\"\",\"password\":\"\",\"ca\":\"\"}}";
    rc = mqtt_option_init(plugin, config11, &option);
    EXPECT_EQ(0, rc);
    mqtt_option_uninit(plugin, &option);

    memset(&option, 0, sizeof(neu_mqtt_option_t));
    char *config12 =
        (char *) "{\"node_id\":5,\"params\":{\"client-id\":\"upload123\", "
                 "\"upload-topic\":\"\", \"heartbeat-topic\":\"\", \"format\": "
                 "0,\"ssl\":true,\"host\":\"192.168.10.116\",\"port\":1883,"
                 "\"username\":\"\",\"password\":\"\",\"ca\":\"\", "
                 "\"cert\":\"\"}}";
    rc = mqtt_option_init(plugin, config12, &option);
    EXPECT_EQ(-4, rc);
    mqtt_option_uninit(plugin, &option);

    memset(&option, 0, sizeof(neu_mqtt_option_t));
    char *config13 =
        (char *) "{\"node_id\":5,\"params\":{\"client-id\":\"upload123\", "
                 "\"upload-topic\":\"\", \"heartbeat-topic\":\"\", \"format\": "
                 "0,\"ssl\":false,\"host\":\"192.168.10.116\",\"port\":1883,"
                 "\"username\":\"\",\"password\":\"\",\"ca\":\"\", "
                 "\"cert\":\"\", \"key\":\"\"}}";
    rc = mqtt_option_init(plugin, config13, &option);
    EXPECT_EQ(0, rc);
    mqtt_option_uninit(plugin, &option);

    memset(&option, 0, sizeof(neu_mqtt_option_t));
    char *config14 =
        (char *) "{\"node_id\":5,\"params\":{\"client-id\":\"upload123\", "
                 "\"upload-topic\":\"\", \"heartbeat-topic\":\"\", \"format\": "
                 "0,\"ssl\":true,\"host\":\"192.168.10.116\",\"port\":1883,"
                 "\"username\":\"\",\"password\":\"\",\"ca\":\"\", "
                 "\"cert\":\"\", \"key\":\"\", \"keypass\":\"\"}}";
    rc = mqtt_option_init(plugin, config14, &option);
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
