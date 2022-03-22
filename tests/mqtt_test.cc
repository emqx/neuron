#include <gtest/gtest.h>

#include "command/node.h"
#include "util.h"
#include <neuron.h>

TEST(MQTTTest, mqtt_option_init_by_config)
{
    neu_config_t config;
    memset(&config, 0, sizeof(neu_config_t));
    neu_mqtt_option_t option;
    memset(&option, 0, sizeof(neu_mqtt_option_t));
    config.buf =
        (char *) "{ \"node_id\": 6, \"params\": { \"req-topic\": "
                 "\"neuronlite/response\", \"res-topic\": "
                 "\"neuronlite/request\", "
                 "\"ssl\": false, \"host\": \"broker.emqx.io\", \"port\": "
                 "\"1883\", "
                 "\"username\": \"\", \"password\": \"\", \"ca-path\": \"\", "
                 "\"ca-file\": \"\" } }";

    int rc = mqtt_option_init(&config, &option);
    EXPECT_EQ(0, rc);
    mqtt_option_uninit(&option);
}
