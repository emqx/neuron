#include <gtest/gtest.h>

#include "config.h"

TEST(JsonTest, DecodeField)
{
    neu_config_init((char *) "./neuron.yaml");
    char *result =
        neu_config_get_value((char *) "./neuron.yaml", 1, (char *) "lib_path");

    EXPECT_STREQ("./", result);
    free(result);

    result = neu_config_get_value((char *) "./neuron.yaml", 3, (char *) "mqtt",
                                  (char *) "broker", (char *) "host");
    EXPECT_STREQ("broker.emqx.io", result);
    free(result);

    result = neu_config_get_value((char *) "./neuron.yaml", 3, (char *) "mqtt",
                                  (char *) "broker", (char *) "port");
    EXPECT_STREQ("1883", result);
    free(result);

    result = neu_config_get_value((char *) "./neuron.yaml", 2, (char *) "web",
                                  (char *) "host");
    EXPECT_STREQ("0.0.0.0", result);
    free(result);

    result = neu_config_get_value((char *) "./neuron.yaml", 2, (char *) "web",
                                  (char *) "port");
    EXPECT_STREQ("7000", result);
    free(result);

    result = neu_config_get_value((char *) "./neuron.yaml", 2, (char *) "api",
                                  (char *) "host");
    EXPECT_STREQ("0.0.0.0", result);
    free(result);

    result = neu_config_get_value((char *) "./neuron.yaml", 2, (char *) "api",
                                  (char *) "port");
    EXPECT_STREQ("7001", result);
    free(result);

    result = neu_config_get_value((char *) "./neuron.yaml", 2, (char *) "api",
                                  (char *) "public_key");
    EXPECT_STREQ("./.ssh/id_rsa.pub", result);
    free(result);

    result = neu_config_get_value((char *) "./neuron.yaml", 2, (char *) "api",
                                  (char *) "private_key");
    EXPECT_STREQ("./.ssh/id_rsa", result);
    free(result);
}
