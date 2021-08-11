#include <gtest/gtest.h>
#include <iostream>
#include <string>

#include "option.h"
#include "paho_client.h"

using namespace std;

class PahoClientTest : public ::testing::Test {
protected:
    PahoClientTest() { }
    ~PahoClientTest() override { }

    void SetUp() override
    {
        clientID   = "neuron-lite-mqtt-plugin";
        topic      = "MQTT Examples";
        qos        = 0;
        connection = "tcp://";
        host       = "broker.emqx.io";
        port       = "1883";
        payload    = "Hello MQTT!";

        option.clientid           = (char *) clientID.c_str();
        option.MQTT_version       = 4;
        option.topic              = (char *) topic.c_str();
        option.qos                = 1;
        option.connection         = (char *) connection.c_str();
        option.host               = (char *) host.c_str();
        option.port               = (char *) port.c_str();
        option.keepalive_interval = 20;
        option.clean_session      = 1;
    }

    void TearDown() override { }

    option_t       option;
    option_t       option1;
    option_t       option2;
    paho_client_t *paho;

    string clientID;
    string connection;
    string host;
    string port;
    string topic;
    int    qos;
    string payload;
};

TEST_F(PahoClientTest, paho_client_open)
{
    EXPECT_EQ(paho_client_open(&option, NULL, &paho), 0);
    EXPECT_NE(paho, nullptr);
}

// TEST_F(PahoClientTest, paho_client_subscribe)
// {
//     EXPECT_EQ(paho_client_open(&option, &paho), 0);
//     EXPECT_NE(paho, nullptr);
//     EXPECT_EQ(paho_client_subscribe(paho, topic.c_str(), qos, NULL, NULL),
//     0);
// }

// TEST_F(PahoClientTest, paho_client_publish)
// {
//     EXPECT_EQ(paho_client_open(&option, &paho), 0);
//     EXPECT_NE(paho, nullptr);
//     EXPECT_EQ(paho_client_subscribe(paho, topic.c_str(), qos, NULL, NULL),
//     0); EXPECT_EQ(paho_client_publish(paho, topic.c_str(), qos,
//                                   (unsigned char *) payload.c_str(),
//                                   strlen(payload.c_str())),
//               0);
//     EXPECT_EQ(paho_client_publish(paho, topic.c_str(), qos,
//                                   (unsigned char *) payload.c_str(),
//                                   strlen(payload.c_str())),
//               0);
//     EXPECT_EQ(paho_client_publish(paho, topic.c_str(), qos,
//                                   (unsigned char *) payload.c_str(),
//                                   strlen(payload.c_str())),
//               0);
// }

// TEST_F(PahoClientTest, paho_client_unsubscribe)
// {
//     EXPECT_EQ(paho_client_open(&option, &paho), 0);
//     EXPECT_NE(paho, nullptr);
//     EXPECT_EQ(paho_client_subscribe(paho, topic.c_str(), qos, NULL, NULL),
//     0); EXPECT_EQ(paho_client_unsubscribe(paho, topic.c_str()), 0);
// }

// TEST_F(PahoClientTest, paho_client_close)
// {
//     EXPECT_EQ(paho_client_open(&option, &paho), 0);
//     EXPECT_NE(paho, nullptr);
//     EXPECT_EQ(paho_client_subscribe(paho, topic.c_str(), qos, NULL, NULL),
//     0); EXPECT_EQ(paho_client_unsubscribe(paho, topic.c_str()), 0);
//     EXPECT_EQ(paho_client_close(paho), 0);
// }