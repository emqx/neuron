#include <sys/time.h>

#include <gtest/gtest.h>

#include "jwt.h"
#include "utils/neu_jwt.h"

#include "utils/log.h"

zlog_category_t *neuron = NULL;
TEST(JwtTest, SetJwt)
{
    char *token = NULL;

    EXPECT_EQ(0, neu_jwt_init((char *) "./config"));

    EXPECT_EQ(0, neu_jwt_new(&token, "admin"));
    EXPECT_GE(strlen(token), 400);

    jwt_free_str(token);
}

TEST(JwtTest, JwtValidate)
{
    char *token         = NULL;
    char  b_token[1024] = { 0 };

    EXPECT_EQ(0, neu_jwt_init((char *) "./config"));
    EXPECT_EQ(0, neu_jwt_new(&token, "admin"));

    snprintf(b_token, sizeof(b_token), "Bearer %s", token);

    EXPECT_EQ(0, neu_jwt_validate(b_token));

    jwt_free_str(token);
}
int main(int argc, char **argv)
{
    zlog_init("./config/dev.conf");
    neuron = zlog_get_category("neuron");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
