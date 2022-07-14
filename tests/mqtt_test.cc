#include <gtest/gtest.h>

#include "mqtt_util.h"
#include <neuron.h>

#include "utils/log.h"

zlog_category_t *neuron = NULL;

TEST(MQTTTest, mqtt_option_init_by_config)
{
    neu_mqtt_option_t option;
    memset(&option, 0, sizeof(neu_mqtt_option_t));
    char *config =
        (char *) "{\"node_id\":5,\"params\":{\"client-id\":"
                 "\"upload123\", \"upload-topic\":\"\", "
                 "\"heartbeat-topic\":\"\", \"format\": "
                 "0,\"ssl\":false,\"host\":\"192.168.10."
                 "116\",\"port\":1883,\"username\":\"\",\"password\":"
                 "\"\",\"ca\":\"\", \"cert\":\"\", \"key\":\"\", "
                 "\"keypass\":\"\"}}";

    int rc = mqtt_option_init(config, &option);
    EXPECT_EQ(0, rc);
    mqtt_option_uninit(&option);
}

int main(int argc, char **argv)
{
    zlog_init("./config/dev.conf");
    neuron = zlog_get_category("neuron");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
