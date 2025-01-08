#include <gtest/gtest.h>

#include "neuron.h"

#include "mqtt/schema.h"

int64_t          global_timestamp = 0;
zlog_category_t *neuron           = NULL;

TEST(validate_success, schema)
{
    const char *schema =
        "{\"timestamp\":\"${timestamp}\",\"node\":\"${node}\",\"group\":\"${"
        "group}\",\"tags\":\"${tag_values}\",\"static\":\"${static_"
        "tags}\",\"errors\":\"${tag_errors}\", \"xxxxx\":\"yyy\"}";
    mqtt_schema_vt_t *vts;
    size_t            n_vts;
    int               ret = mqtt_schema_validate(schema, &vts, &n_vts);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(n_vts, 7);

    EXPECT_STREQ(vts[0].name, "timestamp");
    EXPECT_EQ(vts[0].vt, MQTT_SCHEMA_TIMESTAMP);

    EXPECT_STREQ(vts[1].name, "node");
    EXPECT_EQ(vts[1].vt, MQTT_SCHEMA_NODE_NAME);

    EXPECT_STREQ(vts[2].name, "group");
    EXPECT_EQ(vts[2].vt, MQTT_SCHEMA_GROUP_NAME);

    EXPECT_STREQ(vts[3].name, "tags");
    EXPECT_EQ(vts[3].vt, MQTT_SCHEMA_TAG_VALUES);

    EXPECT_STREQ(vts[4].name, "static");
    EXPECT_EQ(vts[4].vt, MQTT_SCHEMA_STATIC_TAG_VALUES);

    EXPECT_STREQ(vts[5].name, "errors");
    EXPECT_EQ(vts[5].vt, MQTT_SCHEMA_TAG_ERRORS);

    EXPECT_STREQ(vts[6].name, "xxxxx");
    EXPECT_EQ(vts[6].vt, MQTT_SCHEMA_UD);
    EXPECT_STREQ(vts[6].ud, "yyy");

    free(vts);
}

TEST(validate_failed, schema)
{
    const char *schema =
        "{\"timestamp\":\"${timestamp}\",\"node\":\"${node}\",\"group\":\"${"
        "group}\",\"tags\":\"${tag_values1}\",\"static\":\"${static_"
        "tags}\",\"errors\":\"${tag_errors}\", \"xxxxx\":\"yyy\"}";
    mqtt_schema_vt_t *vts;
    size_t            n_vts;
    int               ret = mqtt_schema_validate(schema, &vts, &n_vts);
    EXPECT_EQ(ret, -1);
}

TEST(validate_success, schema_static)
{
    const char *static_tags =
        "{\"static_tags\": {\"static_tag1\":\"string\", "
        "\"static_tag2\": 1234,\"static_tag3\": 11.44, "
        "\"static_tag4\": true, \"static_tag5\": "
        "[1,3,4], \"static_tag6\": {\"sub\": 13}, \"static_tag7\": {\"sub1\": "
        "{\"sub2\": \"string\"}} }}";

    mqtt_static_vt_t *vts = NULL;
    size_t            n_vts;

    int ret = mqtt_static_validate(static_tags, &vts, &n_vts);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(n_vts, 7);

    EXPECT_STREQ(vts[0].name, "static_tag1");
    EXPECT_EQ(vts[0].jtype, NEU_JSON_STR);
    EXPECT_STREQ(vts[0].jvalue.val_str, "string");

    EXPECT_STREQ(vts[1].name, "static_tag2");
    EXPECT_EQ(vts[1].jtype, NEU_JSON_INT);
    EXPECT_EQ(vts[1].jvalue.val_int, 1234);

    EXPECT_STREQ(vts[2].name, "static_tag3");
    EXPECT_EQ(vts[2].jtype, NEU_JSON_DOUBLE);
    EXPECT_EQ(vts[2].jvalue.val_double, 11.44);

    EXPECT_STREQ(vts[3].name, "static_tag4");
    EXPECT_EQ(vts[3].jtype, NEU_JSON_BOOL);
    EXPECT_EQ(vts[3].jvalue.val_bool, true);

    EXPECT_STREQ(vts[4].name, "static_tag5");
    EXPECT_EQ(vts[4].jtype, NEU_JSON_STR);

    EXPECT_STREQ(vts[5].name, "static_tag6");
    EXPECT_EQ(vts[5].jtype, NEU_JSON_STR);

    EXPECT_STREQ(vts[6].name, "static_tag7");
    EXPECT_EQ(vts[6].jtype, NEU_JSON_STR);

    mqtt_static_free(vts, n_vts);
}

TEST(schema_encode, schema_static)
{
    const char *schema =
        "{\"timestamp\":\"${timestamp}\",\"node\":\"${node}\",\"group\":\"${"
        "group}\",\"tags\":\"${tag_values}\",\"static\":\"${static_"
        "tags}\",\"errors\":\"${tag_errors}\", \"xxxxx\":\"yyy\"}";
    mqtt_schema_vt_t *schema_vts;
    size_t            schema_n_vts;
    int ret = mqtt_schema_validate(schema, &schema_vts, &schema_n_vts);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(schema_n_vts, 7);

    const char *static_tags =
        "{\"static_tags\": {\"static_tag1\":\"string\", "
        "\"static_tag2\": 1234,\"static_tag3\": 11.44, "
        "\"static_tag4\": true, \"static_tag5\": "
        "[1,3,4], \"static_tag6\": {\"sub\": 13}, \"static_tag7\": {\"sub1\": "
        "{\"sub2\": \"string\"}} }}";

    mqtt_static_vt_t *vts = NULL;
    size_t            n_vts;

    ret = mqtt_static_validate(static_tags, &vts, &n_vts);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(n_vts, 7);

    char *               result = NULL;
    neu_json_read_resp_t tags   = { 0 };
    ret = mqtt_schema_encode((char *) "driver", (char *) "group", &tags,
                             schema_vts, schema_n_vts, vts, n_vts, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_STREQ(
        result,
        "{\"timestamp\": 0, \"node\": \"driver\", \"group\": \"group\", "
        "\"tags\": [], \"static\": [{\"name\": \"static_tag1\", \"value\": "
        "\"string\"}, {\"name\": \"static_tag2\", \"value\": 1234}, {\"name\": "
        "\"static_tag3\", \"value\": 11.44}, {\"name\": \"static_tag4\", "
        "\"value\": true}, {\"name\": \"static_tag5\", \"value\": \"[1, 3, "
        "4]\"}, {\"name\": \"static_tag6\", \"value\": \"{\\\"sub\\\": 13}\"}, "
        "{\"name\": \"static_tag7\", \"value\": \"{\\\"sub1\\\": "
        "{\\\"sub2\\\": \\\"string\\\"}}\"}], \"errors\": [], \"xxxxx\": "
        "\"yyy\"}");

    mqtt_static_free(vts, n_vts);
    free(schema_vts);
    free(result);
}

int main(int argc, char **argv)
{
    zlog_init("./config/dev.conf");
    neuron = zlog_get_category("neuron");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
