#include <gtest/gtest.h>

#include "open62541_option.h"

TEST(MQTTTest, mqtt_option_init_by_config)
{
    neu_config_t config;
    memset(&config, 0, sizeof(neu_config_t));
    option_t option;
    memset(&option, 0, sizeof(option_t));
    config.buf = (char *) "{\"node_id\": 6, \"params\": {\"host\": "
                          "\"localhost\", \"port\": 4840, \"username\": "
                          "\"test123\", \"password\": \"test123\"}}";

    int rc = opcua_option_init_by_config(&config, &option);
    EXPECT_EQ(0, rc);
    opcua_option_uninit(&option);
}