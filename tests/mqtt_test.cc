#include <gtest/gtest.h>

#include "command/node.h"
#include "mqtt_nng_client.h"
#include "mqtt_nng_util.h"
#include <neuron.h>

TEST(MQTTTest, mqtt_option_init_by_config)
{
    neu_config_t config;
    memset(&config, 0, sizeof(neu_config_t));
    mqtt_option_t option;
    memset(&option, 0, sizeof(mqtt_option_t));
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

TEST(MQTTTest, mqtt_nng_client_open)
{
    char uuid4_str[40] = { '\0' };
    neu_uuid_v4_gen(uuid4_str);

    mqtt_option_t option;
    memset(&option, 0, sizeof(mqtt_option_t));
    option.host         = strdup("192.168.10.127");
    option.port         = strdup("1883");
    option.clientid     = strdup(uuid4_str);
    option.verbose      = 1;
    option.MQTT_version = 4;
    option.connection   = strdup("tcp://");

    mqtt_nng_client_t *client = NULL;
    mqtt_error_e       error  = mqtt_nng_client_open(&client, &option, NULL);
    mqtt_nng_client_subscribe(client, "one/testneuron", 0, NULL);
    char p[10] = "123456";
    mqtt_nng_client_publish(client, "hahah/test", 0, (unsigned char *) p,
                            strlen(p));
    sleep(6);
    EXPECT_EQ(MQTT_SUCCESS, error);
    EXPECT_NE(nullptr, client);
    error = mqtt_nng_client_close(client);
    EXPECT_EQ(MQTT_SUCCESS, error);

    mqtt_option_uninit(&option);
}

// TEST(MQTTTest, mqtt_nng_client_tls)
// {
//     char uuid4_str[40] = { '\0' };
//     neu_uuid_v4_gen(uuid4_str);

//     mqtt_option_t option;
//     memset(&option, 0, sizeof(mqtt_option_t));
//     option.host         = strdup("broker-cn.emqx.io");
//     option.port         = strdup("8883");
//     option.clientid     = strdup(uuid4_str);
//     option.verbose      = 1;
//     option.MQTT_version = 4;
//     option.connection   = strdup("ssl://");
//     option.cafile       = strdup("/home/gc/broker.emqx.io-ca.crt");

//     mqtt_nng_client_t *client = NULL;
//     mqtt_error_e       error  = mqtt_nng_client_open(&client, &option, NULL);
//     mqtt_nng_client_subscribe(client, "one/testneuron", 0, NULL);
//     char p[10] = "123456";
//     mqtt_nng_client_publish(client, "hahah/test", 0, (unsigned char *) p,
//                             strlen(p));
//     sleep(10);
//     EXPECT_EQ(MQTT_SUCCESS, error);
//     EXPECT_NE(nullptr, client);
//     error = mqtt_nng_client_close(client);
//     EXPECT_EQ(MQTT_SUCCESS, error);

//     mqtt_option_uninit(&option);
// }