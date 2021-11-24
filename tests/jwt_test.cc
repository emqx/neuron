#include <gtest/gtest.h>

#include "config.h"
#include "jwt.h"
#include "neu_jwt.h"

TEST(JsonTest, JwtValidate)
{
    char *        token            = NULL;
    unsigned char public_key[4096] = { 0 };
    size_t        pub_key_len      = 0;
    jwt_t *       jwt              = NULL;

    char *public_key_path = neu_config_get_value(
        (char *) "./neuron.yaml", 2, (char *) "api", (char *) "public_key");
    char *private_key_path = neu_config_get_value(
        (char *) "./neuron.yaml", 2, (char *) "api", (char *) "private_key");

    neu_jwt_init(public_key_path, private_key_path);

    neu_jwt_new(&token);
    EXPECT_STREQ("abc", token);

    jwt_valid_t *jwt_valid = NULL;
    jwt_alg_t    opt_alg   = JWT_ALG_RS256;

    jwt_valid_new(&jwt_valid, opt_alg);
    jwt_valid_set_headers(jwt_valid, 1);
    jwt_valid_set_now(jwt_valid, time(NULL));

    jwt_decode(&jwt, token, public_key, pub_key_len);

    jwt_valid_free(jwt_valid);
}