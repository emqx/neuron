#include <gtest/gtest.h>

#include "command/node.h"
#include "mqtt_nng_client.h"
#include "mqttc_util.h"
#include <neuron.h>

TEST(MQTTTest, mqtt_option_init_by_config)
{
    neu_config_t config;
    memset(&config, 0, sizeof(neu_config_t));
    option_t option;
    memset(&option, 0, sizeof(option_t));
    config.buf =
        (char *) "{ \"node_id\": 6, \"params\": { \"req-topic\": "
                 "\"neuronlite/response\", \"res-topic\": "
                 "\"neuronlite/request\", "
                 "\"ssl\": false, \"host\": \"broker.emqx.io\", \"port\": "
                 "\"1883\", "
                 "\"username\": \"\", \"password\": \"\", \"ca-path\": \"\", "
                 "\"ca-file\": \"\" } }";

    int rc = mqtt_option_init_by_config(&config, &option);
    EXPECT_EQ(0, rc);
    mqtt_option_uninit(&option);
}

TEST(MQTTTest, mqtt_nng_client_open)
{
    char uuid4_str[40] = { '\0' };
    neu_uuid_v4_gen(uuid4_str);

    option_t option;
    memset(&option, 0, sizeof(option_t));
    option.host         = strdup("localhost");
    option.port         = strdup("1883");
    option.clientid     = strdup(uuid4_str);
    option.verbose      = 1;
    option.MQTT_version = 4;

    mqtt_nng_client_t *client = NULL;
    client_error_e     error  = mqtt_nng_client_open(&client, &option, NULL);
    sleep(30);
    EXPECT_EQ(MQTTC_SUCCESS, error);
    EXPECT_NE(nullptr, client);
    error = mqtt_nng_client_close(client);
    EXPECT_EQ(MQTTC_SUCCESS, error);

    mqtt_option_uninit(&option);
}