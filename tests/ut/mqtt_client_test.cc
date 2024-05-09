#include <gtest/gtest.h>

#include "connection/mqtt_client.h"
#include "utils/log.h"

zlog_category_t *neuron = NULL;

TEST(MQTTClientTest, test_topic_filter_is_valid)
{
    ASSERT_EQ(false, neu_mqtt_topic_filter_is_valid(NULL));
    ASSERT_EQ(false, neu_mqtt_topic_filter_is_valid(""));

    ASSERT_EQ(false, neu_mqtt_topic_filter_is_valid("sport/tennis#"));
    ASSERT_EQ(false, neu_mqtt_topic_filter_is_valid("sport/tennis/#/ranking"));
    ASSERT_EQ(true, neu_mqtt_topic_filter_is_valid("#"));
    ASSERT_EQ(true, neu_mqtt_topic_filter_is_valid("sport/#"));
    ASSERT_EQ(true, neu_mqtt_topic_filter_is_valid("sport/tennis/player/#"));

    ASSERT_EQ(false, neu_mqtt_topic_filter_is_valid("sport+"));
    ASSERT_EQ(false, neu_mqtt_topic_filter_is_valid("sport/+player"));
    ASSERT_EQ(true, neu_mqtt_topic_filter_is_valid("+"));
    ASSERT_EQ(true, neu_mqtt_topic_filter_is_valid("+/+"));
    ASSERT_EQ(true, neu_mqtt_topic_filter_is_valid("sport/+/player"));
    ASSERT_EQ(true, neu_mqtt_topic_filter_is_valid("+/tennis/#"));

    ASSERT_EQ(true, neu_mqtt_topic_filter_is_valid("$SYS"));
    ASSERT_EQ(true, neu_mqtt_topic_filter_is_valid("$SYS/#"));
    ASSERT_EQ(true, neu_mqtt_topic_filter_is_valid("sport"));
    ASSERT_EQ(true, neu_mqtt_topic_filter_is_valid("sport/tennis"));
    ASSERT_EQ(true, neu_mqtt_topic_filter_is_valid("sport/tennis/player"));
    ASSERT_EQ(true,
              neu_mqtt_topic_filter_is_valid("sport/tennis/player/ranking"));
}

TEST(MQTTClientTest, test_topic_filter_is_match)
{
    const char *filter = "#";
    ASSERT_EQ(false, neu_mqtt_topic_filter_is_match(filter, "$sport"));
    ASSERT_EQ(true, neu_mqtt_topic_filter_is_match(filter, "sport"));
    ASSERT_EQ(true, neu_mqtt_topic_filter_is_match(filter, "sport/tennis"));
    ASSERT_EQ(true,
              neu_mqtt_topic_filter_is_match(filter, "sport/tennis/player"));

    filter = "sport/tennis/player/#";
    ASSERT_EQ(false, neu_mqtt_topic_filter_is_match(filter, "sport"));
    ASSERT_EQ(false, neu_mqtt_topic_filter_is_match(filter, "sport/tennis"));
    ASSERT_EQ(false,
              neu_mqtt_topic_filter_is_match(filter, "sport/tennis/play"));
    ASSERT_EQ(true,
              neu_mqtt_topic_filter_is_match(filter, "sport/tennis/player"));
    ASSERT_EQ(true,
              neu_mqtt_topic_filter_is_match(filter, "sport/tennis/player/"));
    ASSERT_EQ(
        true,
        neu_mqtt_topic_filter_is_match(filter, "sport/tennis/player/ranking"));
    ASSERT_EQ(true,
              neu_mqtt_topic_filter_is_match(
                  filter, "sport/tennis/player/ranking/score/wimbledon"));

    filter = "+";
    ASSERT_EQ(false, neu_mqtt_topic_filter_is_match(filter, "$finance"));
    ASSERT_EQ(false, neu_mqtt_topic_filter_is_match(filter, "/finance"));
    ASSERT_EQ(true, neu_mqtt_topic_filter_is_match(filter, "finance"));

    filter = "/+";
    ASSERT_EQ(false, neu_mqtt_topic_filter_is_match(filter, "finance"));
    ASSERT_EQ(true, neu_mqtt_topic_filter_is_match(filter, "/finance"));

    filter = "+/+";
    ASSERT_EQ(false, neu_mqtt_topic_filter_is_match(filter, "$finance"));
    ASSERT_EQ(false, neu_mqtt_topic_filter_is_match(filter, "finance"));
    ASSERT_EQ(true, neu_mqtt_topic_filter_is_match(filter, "/finance"));
    ASSERT_EQ(true, neu_mqtt_topic_filter_is_match(filter, "sport/tennis"));

    filter = "sport/+";
    ASSERT_EQ(false, neu_mqtt_topic_filter_is_match(filter, "sport"));
    ASSERT_EQ(true, neu_mqtt_topic_filter_is_match(filter, "sport/"));
    ASSERT_EQ(true, neu_mqtt_topic_filter_is_match(filter, "sport/tennis"));
    ASSERT_EQ(false, neu_mqtt_topic_filter_is_match(filter, "sport/tennis/"));
    ASSERT_EQ(false,
              neu_mqtt_topic_filter_is_match(filter, "sport/tennis/ranking"));

    filter = "sport/tennis/+";
    ASSERT_EQ(false, neu_mqtt_topic_filter_is_match(filter, "sport"));
    ASSERT_EQ(false, neu_mqtt_topic_filter_is_match(filter, "sport/"));
    ASSERT_EQ(false, neu_mqtt_topic_filter_is_match(filter, "sport/tennis"));
    ASSERT_EQ(
        false,
        neu_mqtt_topic_filter_is_match(filter, "sport/tennis/player/ranking"));
    ASSERT_EQ(true,
              neu_mqtt_topic_filter_is_match(filter, "sport/tennis/player1"));
    ASSERT_EQ(true,
              neu_mqtt_topic_filter_is_match(filter, "sport/tennis/player2"));

    filter = "sport/+/player";
    ASSERT_EQ(false, neu_mqtt_topic_filter_is_match(filter, "sport"));
    ASSERT_EQ(false, neu_mqtt_topic_filter_is_match(filter, "sport/"));
    ASSERT_EQ(false, neu_mqtt_topic_filter_is_match(filter, "sport/tennis"));
    ASSERT_EQ(false, neu_mqtt_topic_filter_is_match(filter, "sport/tennis/"));
    ASSERT_EQ(false,
              neu_mqtt_topic_filter_is_match(filter, "sport/tennis/ranking"));
    ASSERT_EQ(true,
              neu_mqtt_topic_filter_is_match(filter, "sport/tennis/player"));
    ASSERT_EQ(false,
              neu_mqtt_topic_filter_is_match(filter, "sport/tennis/player/"));
    ASSERT_EQ(
        false,
        neu_mqtt_topic_filter_is_match(filter, "sport/tennis/player/ranking"));

    filter = "+/tennis/#";
    ASSERT_EQ(false, neu_mqtt_topic_filter_is_match(filter, "sport"));
    ASSERT_EQ(false, neu_mqtt_topic_filter_is_match(filter, "sport/"));
    ASSERT_EQ(false, neu_mqtt_topic_filter_is_match(filter, "sport/baseball"));
    ASSERT_EQ(true, neu_mqtt_topic_filter_is_match(filter, "sport/tennis"));
    ASSERT_EQ(true, neu_mqtt_topic_filter_is_match(filter, "sport/tennis/"));
    ASSERT_EQ(true,
              neu_mqtt_topic_filter_is_match(filter, "sport/tennis/player"));
    ASSERT_EQ(
        true,
        neu_mqtt_topic_filter_is_match(filter, "sport/tennis/player/ranking"));
}

int main(int argc, char **argv)
{
    zlog_init("./config/dev.conf");
    neuron = zlog_get_category("neuron");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
