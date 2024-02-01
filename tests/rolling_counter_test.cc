#include <gtest/gtest.h>

#include "utils/log.h"
#include "utils/rolling_counter.h"

zlog_category_t *neuron = NULL;
TEST(RollingCounterTest, neu_rolling_counter_inc)
{
    uint64_t               ts      = 0;
    neu_rolling_counter_t *counter = neu_rolling_counter_new(4000);
    EXPECT_NE(nullptr, counter);

    for (int i = 1; i <= 4; ++i) {
        ts += 1000;
        EXPECT_EQ(i, neu_rolling_counter_inc(counter, ts, 1));
    }

    // wrap around
    for (int i = 1; i <= 4; ++i) {
        ts += 1000;
        EXPECT_EQ(4, neu_rolling_counter_inc(counter, ts, 1));
    }

    // increment without time stamp update
    EXPECT_EQ(4, neu_rolling_counter_value(counter));
    for (int i = 1; i <= 10; ++i) {
        EXPECT_EQ(4 + i, neu_rolling_counter_inc(counter, ts, 1));
    }

    EXPECT_EQ(4 + 10, neu_rolling_counter_value(counter));

    // stale value will be clean
    ts += 4000;
    EXPECT_EQ(0, neu_rolling_counter_inc(counter, ts, 0));

    neu_rolling_counter_free(counter);
}

int main(int argc, char **argv)
{
    zlog_init("./config/dev.conf");
    neuron = zlog_get_category("neuron");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
