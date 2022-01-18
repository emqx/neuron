#include <gtest/gtest.h>

#include "config.h"

TEST(ConfigTest, ReadConfig)
{
    neu_config_init((char *) "./neuron.yaml");

    char *result = neu_config_get_value(2, (char *) "web", (char *) "host");
    EXPECT_STREQ("0.0.0.0", result);
    free(result);

    result = neu_config_get_value(2, (char *) "web", (char *) "port");
    EXPECT_STREQ("7000", result);
    free(result);

    result = neu_config_get_value(2, (char *) "api", (char *) "host");
    EXPECT_STREQ("0.0.0.0", result);
    free(result);

    result = neu_config_get_value(2, (char *) "api", (char *) "port");
    EXPECT_STREQ("7001", result);
    free(result);

    result = neu_config_get_value(2, (char *) "api", (char *) "public_key");
    EXPECT_STREQ("./public_test.pem", result);
    free(result);

    result = neu_config_get_value(2, (char *) "api", (char *) "private_key");
    EXPECT_STREQ("./private_test.key", result);
    free(result);
}
