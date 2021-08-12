#include <gtest/gtest.h>

#include "utils/json.h"

TEST(JsonTest, DecodeField)
{
    char *buf =
        (char *) "{\"field1\": true, \"field2\": false, \"field3\": 12345, "
                 "\"field4\": 11.223, \"field5\": \"hello world\"}";

    neu_json_elem_t elems[] = { { .name = (char *) "field1",
                                  .t    = NEU_JSON_BOOL },
                                {
                                    .name = (char *) "field2",
                                    .t    = NEU_JSON_BOOL,
                                },
                                {
                                    .name = (char *) "field3",
                                    .t    = NEU_JSON_INT,
                                },
                                {
                                    .name = (char *) "field4",
                                    .t    = NEU_JSON_DOUBLE,
                                },
                                {
                                    .name = (char *) "field5",
                                    .t    = NEU_JSON_STR,
                                } };
    EXPECT_EQ(5, sizeof(elems) / sizeof(neu_json_elem_t));
    EXPECT_EQ(5, NEU_JSON_ELEM_SIZE(elems));
    EXPECT_EQ(0, neu_json_decode(buf, NEU_JSON_ELEM_SIZE(elems), elems));
    EXPECT_EQ(elems[0].v.val_bool, true);
    EXPECT_EQ(elems[1].v.val_bool, false);
    EXPECT_EQ(elems[2].v.val_int, 12345);
    EXPECT_EQ(elems[3].v.val_double, 11.223);
    EXPECT_STREQ(elems[4].v.val_str, "hello world");

    free(elems[4].v.val_str);
}

TEST(JsonTest, DecodeArray)
{
    char *buf =
        (char *) "{\"field1\": 12345, \"field2\": [\"name1\", "
                 "\"name2\", \"name3\"], \"field3\":[{\"name\": \"name1\", "
                 "\"value\": 123},{\"name\":\"name2\", \"value\": "
                 "456},{\"name\":\"name3\", \"value\": 789}] }";

    neu_json_elem_t elems[] = { { .name = (char *) "field1",
                                  .t    = NEU_JSON_INT } };
    neu_json_elem_t names[] = { { .name = NULL, .t = NEU_JSON_STR } };
    neu_json_elem_t tags[]  = { { .name = (char *) "name", .t = NEU_JSON_STR },
                               { .name = (char *) "value",
                                 .t    = NEU_JSON_INT } };

    EXPECT_EQ(1, sizeof(elems) / sizeof(neu_json_elem_t));
    EXPECT_EQ(3, neu_json_decode_array_size(buf, (char *) "field2"));
    EXPECT_EQ(3, neu_json_decode_array_size(buf, (char *) "field3"));

    EXPECT_EQ(0, neu_json_decode_array(buf, (char *) "field2", 0, 1, names));
    EXPECT_STREQ("name1", names[0].v.val_str);
    free(names[0].v.val_str);

    EXPECT_EQ(0, neu_json_decode_array(buf, (char *) "field2", 1, 1, names));
    EXPECT_STREQ("name2", names[0].v.val_str);
    free(names[0].v.val_str);

    EXPECT_EQ(0, neu_json_decode_array(buf, (char *) "field2", 2, 1, names));
    EXPECT_STREQ("name3", names[0].v.val_str);
    free(names[0].v.val_str);

    EXPECT_EQ(0, neu_json_decode_array(buf, (char *) "field3", 0, 2, tags));
    EXPECT_STREQ("name1", tags[0].v.val_str);
    free(tags[0].v.val_str);
    EXPECT_EQ(123, tags[1].v.val_int);

    EXPECT_EQ(0, neu_json_decode_array(buf, (char *) "field3", 1, 2, tags));
    EXPECT_STREQ("name2", tags[0].v.val_str);
    free(tags[0].v.val_str);
    EXPECT_EQ(456, tags[1].v.val_int);

    EXPECT_EQ(0, neu_json_decode_array(buf, (char *) "field3", 2, 2, tags));
    EXPECT_STREQ("name3", tags[0].v.val_str);
    free(tags[0].v.val_str);
    EXPECT_EQ(789, tags[1].v.val_int);
}

TEST(JsonTest, DecodeUndefine)
{
    char *buf =
        (char *) "{\"field1\": true, \"field2\": false, \"field3\": 12345, "
                 "\"field4\": 11.223, \"field5\": \"hello world\"}";

    neu_json_elem_t elems[] = { { .name = (char *) "field1",
                                  .t    = NEU_JSON_UNDEFINE },
                                {
                                    .name = (char *) "field2",
                                    .t    = NEU_JSON_UNDEFINE,
                                },
                                {
                                    .name = (char *) "field3",
                                    .t    = NEU_JSON_UNDEFINE,
                                },
                                {
                                    .name = (char *) "field4",
                                    .t    = NEU_JSON_UNDEFINE,
                                },
                                {
                                    .name = (char *) "field5",
                                    .t    = NEU_JSON_UNDEFINE,
                                } };
    EXPECT_EQ(5, sizeof(elems) / sizeof(neu_json_elem_t));
    EXPECT_EQ(5, NEU_JSON_ELEM_SIZE(elems));
    EXPECT_EQ(0, neu_json_decode(buf, NEU_JSON_ELEM_SIZE(elems), elems));
    EXPECT_EQ(elems[0].t, NEU_JSON_BOOL);
    EXPECT_EQ(elems[0].v.val_bool, true);
    EXPECT_EQ(elems[1].t, NEU_JSON_BOOL);
    EXPECT_EQ(elems[1].v.val_bool, false);
    EXPECT_EQ(elems[2].t, NEU_JSON_INT);
    EXPECT_EQ(elems[2].v.val_int, 12345);
    EXPECT_EQ(elems[3].t, NEU_JSON_DOUBLE);
    EXPECT_EQ(elems[3].v.val_double, 11.223);
    EXPECT_EQ(elems[4].t, NEU_JSON_STR);
    EXPECT_STREQ(elems[4].v.val_str, "hello world");

    free(elems[4].v.val_str);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}