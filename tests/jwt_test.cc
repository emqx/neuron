#include <sys/time.h>

#include <gtest/gtest.h>

#include "config.h"
#include "jwt.h"
#include "neu_jwt.h"
#include "neu_log.h"

TEST(JsonTest, SetJwt)
{
    char *        token            = NULL;
    unsigned char public_key[4096] = { 0 };
    size_t        pub_key_len      = 0;

    char *public_key_path = neu_config_get_value(
        (char *) "./neuron.yaml", 2, (char *) "api", (char *) "public_key");
    char *private_key_path = neu_config_get_value(
        (char *) "./neuron.yaml", 2, (char *) "api", (char *) "private_key");

    EXPECT_EQ(0, neu_jwt_init(public_key_path, private_key_path));

    EXPECT_EQ(0, neu_jwt_new(&token));
    EXPECT_GE(strlen(token), 400);

    jwt_free_str(token);
    free(public_key_path);
    free(private_key_path);
}

TEST(JsonTest, JwtValidate)
{
    char *         token            = NULL;
    char           b_token[1024]    = { 0 };
    unsigned char  public_key[4096] = { 0 };
    size_t         pub_key_len      = 0;
    struct timeval expire_tv        = { 0 };

    char *private_key_path = neu_config_get_value(
        (char *) "./neuron.yaml", 2, (char *) "api", (char *) "private_key");
    char *public_key_path = neu_config_get_value(
        (char *) "./neuron.yaml", 2, (char *) "api", (char *) "public_key");

    EXPECT_EQ(0, neu_jwt_init(public_key_path, private_key_path));
    EXPECT_EQ(0, neu_jwt_new(&token));

    snprintf(b_token, sizeof(b_token), "Bearer %s", token);

    EXPECT_EQ(0, neu_jwt_validate(b_token));

    jwt_free_str(token);
    free(public_key_path);
    free(private_key_path);
}