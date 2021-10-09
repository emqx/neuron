#include <gtest/gtest.h>

#include "neu_uuid.h"

TEST(UUIDTest, neu_uuid_v4_gen)
{
    char uuid4_str[40] = { '\0' };
    int  rc            = neu_uuid_v4_gen(uuid4_str);
    EXPECT_EQ(1, rc);
}