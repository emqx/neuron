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
        string clientID("neuron-lite-mqtt-plugin");
        string topic("MQTT Examples");
        string connection("tcp://");
        string host("192.168.50.165");
        string port("1883");

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
};

TEST_F(PahoClientTest, paho_client_open)
{
    EXPECT_EQ(paho_client_open(&option, &paho), 0);
}