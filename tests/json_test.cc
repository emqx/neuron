#include <gtest/gtest.h>

#include "json/json.h"

#include "utils/log.h"

zlog_category_t *neuron = NULL;
TEST(JsonTest, DecodeField)
{
    char *buf =
        (char *) "{\"field1\": true, \"field2\": false, \"field3\": 12345, "
                 "\"field4\": 11.223, \"field5\": \"hello world\"}";

    neu_json_elem_t elems[5];

    elems[0].name = (char *) "field1";
    elems[0].t    = NEU_JSON_BOOL;

    elems[1].name = (char *) "field2";
    elems[1].t    = NEU_JSON_BOOL;

    elems[2].name = (char *) "field3";
    elems[2].t    = NEU_JSON_INT;

    elems[3].name = (char *) "field4";
    elems[3].t    = NEU_JSON_DOUBLE;

    elems[4].name = (char *) "field5";
    elems[4].t    = NEU_JSON_STR;

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

    neu_json_elem_t elems[1];
    elems[0].name = (char *) "field1";
    elems[0].t    = NEU_JSON_INT;

    neu_json_elem_t names[1];
    names[0].name = NULL;
    names[0].t    = NEU_JSON_STR;

    neu_json_elem_t tags[2];
    tags[0].name = (char *) "name";
    tags[0].t    = NEU_JSON_STR;

    tags[1].name = (char *) "value";
    tags[1].t    = NEU_JSON_INT;

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

    neu_json_elem_t elems[5];

    elems[0].name = (char *) "field1";
    elems[0].t    = NEU_JSON_UNDEFINE;

    elems[1].name = (char *) "field2";
    elems[1].t    = NEU_JSON_UNDEFINE;

    elems[2].name = (char *) "field3";
    elems[2].t    = NEU_JSON_UNDEFINE;

    elems[3].name = (char *) "field4";
    elems[3].t    = NEU_JSON_UNDEFINE;

    elems[4].name = (char *) "field5";
    elems[4].t    = NEU_JSON_UNDEFINE;

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

    neu_json_elem_t elems[5];

    elems[0].name = (char *) "field1";
    elems[0].t    = NEU_JSON_BOOL;

    elems[1].name = (char *) "field2";
    elems[1].t    = NEU_JSON_BOOL;

    elems[2].name = (char *) "field3";
    elems[2].t    = NEU_JSON_INT;

    elems[3].name = (char *) "field4";
    elems[3].t    = NEU_JSON_DOUBLE;

    elems[4].name = (char *) "field5";
    elems[4].t    = NEU_JSON_STR;

    elems[0].v.val_bool   = true;
    elems[1].v.val_bool   = false;
    elems[2].v.val_int    = 12345;
    elems[3].v.val_double = 11.223;
    elems[4].v.val_str    = (char *) "hello world";
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

    neu_json_elem_t array1[3];
    array1[0].name = NULL;
    array1[0].t    = NEU_JSON_STR;

    array1[1].name = NULL;
    array1[1].t    = NEU_JSON_STR;

    array1[2].name = NULL;
    array1[2].t    = NEU_JSON_STR;

    neu_json_elem_t array2[2];
    array2[0].name = (char *) "name";
    array2[0].t    = NEU_JSON_STR;

    array2[1].name = (char *) "value";
    array2[1].t    = NEU_JSON_INT;

    neu_json_elem_t elems[3];
    elems[0].name = (char *) "field1";
    elems[0].t    = NEU_JSON_INT;

    elems[1].name = (char *) "field2";
    elems[1].t    = NEU_JSON_OBJECT;

    elems[2].name = (char *) "field3";
    elems[2].t    = NEU_JSON_OBJECT;

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

int main(int argc, char **argv)
{
    zlog_init("./config/dev.conf");
    neuron = zlog_get_category("neuron");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}