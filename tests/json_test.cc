#include <gtest/gtest.h>

#include "json/json.h"

TEST(JsonTest, DecodeField)
{
    char *buf =
        (char *) "{\"field1\": true, \"field2\": false, \"field3\": 12345, "
                 "\"field4\": 11.223, \"field5\": \"hello world\"}";

    neu_json_elem_t elems[] = { {
                                    .name = (char *) "field1",
                                    .t    = NEU_JSON_BOOL,
                                },
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

    neu_json_elem_t elems[] = { {
        .name = (char *) "field1",
        .t    = NEU_JSON_INT,
    } };
    neu_json_elem_t names[] = { {
        .name = NULL,
        .t    = NEU_JSON_STR,
    } };
    neu_json_elem_t tags[]  = { {
                                   .name = (char *) "name",
                                   .t    = NEU_JSON_STR,
                               },
                               {
                                   .name = (char *) "value",
                                   .t    = NEU_JSON_INT,
                               } };

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

    neu_json_elem_t elems[] = { {
                                    .name = (char *) "field1",
                                    .t    = NEU_JSON_UNDEFINE,
                                },
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

TEST(JsonTest, EncodeField)
{
    char *buf =
        (char *) "{\"field1\": true, \"field2\": false, \"field3\": 12345, "
                 "\"field4\": 11.223, \"field5\": \"hello world\"}";
    char *result = NULL;

    neu_json_elem_t elems[] = { {
                                    .name = (char *) "field1",
                                    .t    = NEU_JSON_BOOL,
                                },
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
    elems[0].v.val_bool     = true;
    elems[1].v.val_bool     = false;
    elems[2].v.val_int      = 12345;
    elems[3].v.val_double   = 11.223;
    elems[4].v.val_str      = (char *) "hello world";
    EXPECT_EQ(5, sizeof(elems) / sizeof(neu_json_elem_t));
    EXPECT_EQ(5, NEU_JSON_ELEM_SIZE(elems));
    void *ob = neu_json_encode_new();
    EXPECT_EQ(0, neu_json_encode_field(ob, elems, NEU_JSON_ELEM_SIZE(elems)));
    EXPECT_EQ(0, neu_json_encode(ob, &result));
    EXPECT_STREQ(result, buf);

    free(result);
}

TEST(JsonTest, EncodeArray)
{
    char *buf =
        (char *) "{\"field1\": 12345, \"field2\": [\"name1\", \"name2\", "
                 "\"name3\"], \"field3\": [{\"name\": \"name1\", \"value\": "
                 "123}, {\"name\": \"name2\", \"value\": 456}]}";
    char *result      = NULL;
    void *json_array1 = NULL;
    void *json_array2 = NULL;

    neu_json_elem_t array1[] = { {
                                     .name = NULL,
                                     .t    = NEU_JSON_STR,
                                 },
                                 {
                                     .name = NULL,
                                     .t    = NEU_JSON_STR,
                                 },
                                 {
                                     .name = NULL,
                                     .t    = NEU_JSON_STR,
                                 } };
    neu_json_elem_t array2[] = {
        {
            .name = (char *) "name",
            .t    = NEU_JSON_STR,
        },
        {
            .name = (char *) "value",
            .t    = NEU_JSON_INT,
        },
    };

    neu_json_elem_t elems[] = { {
                                    .name = (char *) "field1",
                                    .t    = NEU_JSON_INT,
                                },
                                {
                                    .name = (char *) "field2",
                                    .t    = NEU_JSON_OBJECT,
                                },
                                {
                                    .name = (char *) "field3",
                                    .t    = NEU_JSON_OBJECT,
                                } };

    array1[0].v.val_str = (char *) "name1";
    array1[1].v.val_str = (char *) "name2";
    array1[2].v.val_str = (char *) "name3";

    json_array1 =
        neu_json_encode_array_value(NULL, array1, NEU_JSON_ELEM_SIZE(array1));

    array2[0].v.val_str = (char *) "name1";
    array2[1].v.val_int = 123;

    json_array2 =
        neu_json_encode_array(NULL, array2, NEU_JSON_ELEM_SIZE(array2));

    array2[0].v.val_str = (char *) "name2";
    array2[1].v.val_int = 456;

    json_array2 =
        neu_json_encode_array(json_array2, array2, NEU_JSON_ELEM_SIZE(array2));

    elems[0].v.val_int    = 12345;
    elems[1].v.val_object = json_array1;
    elems[2].v.val_object = json_array2;
    EXPECT_EQ(3, NEU_JSON_ELEM_SIZE(elems));
    void *ob = neu_json_encode_new();
    EXPECT_EQ(0, neu_json_encode_field(ob, elems, NEU_JSON_ELEM_SIZE(elems)));
    EXPECT_EQ(0, neu_json_encode(ob, &result));
    EXPECT_STREQ(result, buf);

    free(result);
}
