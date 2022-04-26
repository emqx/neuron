#include <sys/time.h>

#include <gtest/gtest.h>

#include "config.h"
#include "jwt.h"
#include "log.h"
#include "neu_jwt.h"

TEST(JwtTest, SetJwt)
{
    char *        token            = NULL;
    unsigned char public_key[4096] = { 0 };
    size_t        pub_key_len      = 0;

    EXPECT_EQ(0, neu_jwt_init((char *) "./config"));

    EXPECT_EQ(0, neu_jwt_new(&token));
    EXPECT_GE(strlen(token), 400);

    jwt_free_str(token);
}

TEST(JwtTest, JwtValidate)
{
    char *         token            = NULL;
    char           b_token[1024]    = { 0 };
    unsigned char  public_key[4096] = { 0 };
    size_t         pub_key_len      = 0;
    struct timeval expire_tv        = { 0 };

    EXPECT_EQ(0, neu_jwt_init((char *) "./config"));
    EXPECT_EQ(0, neu_jwt_new(&token));

    snprintf(b_token, sizeof(b_token), "Bearer %s", token);

    EXPECT_EQ(0, neu_jwt_validate(b_token));

    jwt_free_str(token);
}