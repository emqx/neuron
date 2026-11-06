#include <cmath>
#include <gtest/gtest.h>
#include <jansson.h>
#include <memory>
#include <string>

#include "parser/neu_json_node.h"
#include "utils/log.h"
#include "json/json.h"

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

TEST(JsonTest, DecodeArray1)
{
    char *buf =
        (char *) "{\"field1\": [1,2,3],\"field2\": [-1,0,1], \"field3\": [1.1, "
                 "2.1, -3.1], \"field4\": [true, false, true]}";
    neu_json_elem_t elems[11];
    elems[0].name = (char *) "field1";
    elems[0].t    = NEU_JSON_ARRAY_UINT8;
    elems[1].name = (char *) "field2";
    elems[1].t    = NEU_JSON_ARRAY_INT8;
    elems[2].name = (char *) "field1";
    elems[2].t    = NEU_JSON_ARRAY_UINT16;
    elems[3].name = (char *) "field2";
    elems[3].t    = NEU_JSON_ARRAY_INT16;
    elems[4].name = (char *) "field1";
    elems[4].t    = NEU_JSON_ARRAY_UINT32;
    elems[5].name = (char *) "field2";
    elems[5].t    = NEU_JSON_ARRAY_INT32;
    elems[6].name = (char *) "field1";
    elems[6].t    = NEU_JSON_ARRAY_UINT64;
    elems[7].name = (char *) "field2";
    elems[7].t    = NEU_JSON_ARRAY_INT64;

    elems[8].name  = (char *) "field3";
    elems[8].t     = NEU_JSON_ARRAY_FLOAT;
    elems[9].name  = (char *) "field3";
    elems[9].t     = NEU_JSON_ARRAY_DOUBLE;
    elems[10].name = (char *) "field4";
    elems[10].t    = NEU_JSON_ARRAY_BOOL;

    EXPECT_EQ(11, sizeof(elems) / sizeof(neu_json_elem_t));
    EXPECT_EQ(11, NEU_JSON_ELEM_SIZE(elems));
    EXPECT_EQ(0, neu_json_decode(buf, NEU_JSON_ELEM_SIZE(elems), elems));

    EXPECT_EQ(1, elems[0].v.val_array_uint8.u8s[0]);
    EXPECT_EQ(2, elems[0].v.val_array_uint8.u8s[1]);
    EXPECT_EQ(3, elems[0].v.val_array_uint8.u8s[2]);

    EXPECT_EQ(-1, elems[1].v.val_array_int8.i8s[0]);
    EXPECT_EQ(0, elems[1].v.val_array_int8.i8s[1]);
    EXPECT_EQ(1, elems[1].v.val_array_int8.i8s[2]);

    EXPECT_EQ(1, elems[2].v.val_array_uint16.u16s[0]);
    EXPECT_EQ(2, elems[2].v.val_array_uint16.u16s[1]);
    EXPECT_EQ(3, elems[2].v.val_array_uint16.u16s[2]);

    EXPECT_EQ(-1, elems[3].v.val_array_int16.i16s[0]);
    EXPECT_EQ(0, elems[3].v.val_array_int16.i16s[1]);
    EXPECT_EQ(1, elems[3].v.val_array_int16.i16s[2]);

    EXPECT_EQ(1, elems[4].v.val_array_uint32.u32s[0]);
    EXPECT_EQ(2, elems[4].v.val_array_uint32.u32s[1]);
    EXPECT_EQ(3, elems[4].v.val_array_uint32.u32s[2]);

    EXPECT_EQ(-1, elems[5].v.val_array_int32.i32s[0]);
    EXPECT_EQ(0, elems[5].v.val_array_int32.i32s[1]);
    EXPECT_EQ(1, elems[5].v.val_array_int32.i32s[2]);

    EXPECT_EQ(1, elems[6].v.val_array_uint64.u64s[0]);
    EXPECT_EQ(2, elems[6].v.val_array_uint64.u64s[1]);
    EXPECT_EQ(3, elems[6].v.val_array_uint64.u64s[2]);

    EXPECT_EQ(-1, elems[7].v.val_array_int64.i64s[0]);
    EXPECT_EQ(0, elems[7].v.val_array_int64.i64s[1]);
    EXPECT_EQ(1, elems[7].v.val_array_int64.i64s[2]);

    EXPECT_FLOAT_EQ(1.1, elems[8].v.val_array_float.f32s[0]);
    EXPECT_FLOAT_EQ(2.1, elems[8].v.val_array_float.f32s[1]);
    EXPECT_FLOAT_EQ(-3.1, elems[8].v.val_array_float.f32s[2]);

    EXPECT_DOUBLE_EQ(1.1, elems[9].v.val_array_double.f64s[0]);
    EXPECT_DOUBLE_EQ(2.1, elems[9].v.val_array_double.f64s[1]);
    EXPECT_DOUBLE_EQ(-3.1, elems[9].v.val_array_double.f64s[2]);

    EXPECT_EQ(true, elems[10].v.val_array_bool.bools[0]);
    EXPECT_EQ(false, elems[10].v.val_array_bool.bools[1]);
    EXPECT_EQ(true, elems[10].v.val_array_bool.bools[2]);

    for (size_t i = 0; i < NEU_JSON_ELEM_SIZE(elems); i++) {
        /* code */
        neu_json_elem_free(&elems[i]);
    }
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

    elems[3].name      = (char *) "field4";
    elems[3].t         = NEU_JSON_DOUBLE;
    elems[3].precision = 0;

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
    neu_json_decode_free(ob);
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
    neu_json_decode_free(ob);
}

namespace {

using PropertyJson = std::unique_ptr<json_t, decltype(&json_decref)>;

const char *property_values[] = {
    R"("text")",
    R"("")",
    "true",
    "false",
    "0",
    "-1",
    "-9223372036854775808",
    "9223372036854775807",
    "9007199254740993",
    "1.25",
    "0.0",
    "-0.0",
    "1.7976931348623157e308",
    "2.2250738585072014e-308",
    "5e-324",
    R"(["", "中文", "a\"b", "a\u0000b"])",
    "[true, false]",
    "[-9223372036854775808, 0, 9223372036854775807]",
    "[0.0, -1.5, 2.5]",
    "[0, 1.5, -2, 0.0]",
    "[]",
    R"("中文\"\\\n\r\t\b\f\u0000end")",
};

void expect_property_json(const char *expected_text, const char *actual_text)
{
    PropertyJson expected(
        json_loads(expected_text, JSON_DECODE_ANY | JSON_ALLOW_NUL, NULL),
        json_decref);
    PropertyJson actual(
        json_loads(actual_text, JSON_DECODE_ANY | JSON_ALLOW_NUL, NULL),
        json_decref);
    ASSERT_NE(nullptr, expected.get());
    ASSERT_NE(nullptr, actual.get());
    EXPECT_EQ(json_typeof(expected.get()), json_typeof(actual.get()));
    EXPECT_TRUE(json_equal(expected.get(), actual.get()));
    if (json_is_real(expected.get()) && json_real_value(expected.get()) == 0) {
        EXPECT_EQ(std::signbit(json_real_value(expected.get())),
                  std::signbit(json_real_value(actual.get())));
    }
}

void expect_property_response(neu_node_property_t *properties, size_t count)
{
    neu_json_get_node_properties_resp_t response = { count, properties };
    char *                              encoded  = NULL;
    ASSERT_EQ(0, neu_json_encode_get_node_properties_resp(&response, &encoded));
    std::unique_ptr<char, decltype(&free)> buffer(encoded, free);
    ASSERT_NE(nullptr, buffer.get());
    PropertyJson decoded(json_loads(buffer.get(), JSON_ALLOW_NUL, NULL),
                         json_decref);
    ASSERT_NE(nullptr, decoded.get());
    EXPECT_EQ(1, json_object_size(decoded.get()));
    json_t *array = json_object_get(decoded.get(), "properties");
    ASSERT_TRUE(json_is_array(array));
    ASSERT_EQ(count, json_array_size(array));
    for (size_t i = 0; i < count; ++i) {
        json_t *item = json_array_get(array, i);
        EXPECT_EQ(2, json_object_size(item));
        ASSERT_TRUE(json_is_string(json_object_get(item, "key")));
        EXPECT_STREQ(properties[i].key,
                     json_string_value(json_object_get(item, "key")));
        char *value =
            neu_node_property_json_dump(json_object_get(item, "value"));
        ASSERT_NE(nullptr, value);
        expect_property_json(properties[i].value_json, value);
        free(value);
    }
}

} // namespace

TEST(JsonTest, NodePropertiesAddAndResponsePreserveTypes)
{
    const size_t count = sizeof(property_values) / sizeof(property_values[0]);
    std::string  input = R"({"node":"driver","properties":[)";
    for (size_t i = 0; i < count; ++i) {
        if (i != 0) {
            input += ",";
        }
        input += "{\"key\":\"key" + std::to_string(i) +
            "\",\"value\":" + property_values[i] + "}";
    }
    input += "]}";
    neu_json_add_node_properties_req_t *raw = NULL;
    ASSERT_EQ(0,
              neu_json_decode_add_node_properties_req(
                  const_cast<char *>(input.c_str()), &raw));
    std::unique_ptr<neu_json_add_node_properties_req_t,
                    decltype(&neu_json_decode_add_node_properties_req_free)>
        request(raw, neu_json_decode_add_node_properties_req_free);
    ASSERT_NE(nullptr, request.get());
    EXPECT_STREQ("driver", request->node);
    ASSERT_EQ(count, request->n_property);
    for (size_t i = 0; i < request->n_property; ++i) {
        SCOPED_TRACE(property_values[i]);
        EXPECT_EQ("key" + std::to_string(i), request->properties[i].key);
        expect_property_json(property_values[i],
                             request->properties[i].value_json);
    }
    expect_property_response(request->properties, request->n_property);
}

TEST(JsonTest, NodePropertyUpdateAndResponsePreserveTypes)
{
    for (const char *value : property_values) {
        SCOPED_TRACE(value);
        std::string input = R"({"node":"app","key":"转义\"\\\n","value":)";
        input += value;
        input += "}";
        neu_json_update_node_property_req_t *raw = NULL;
        ASSERT_EQ(0,
                  neu_json_decode_update_node_property_req(
                      const_cast<char *>(input.c_str()), &raw));
        std::unique_ptr<neu_json_update_node_property_req_t,
                        decltype(
                            &neu_json_decode_update_node_property_req_free)>
            request(raw, neu_json_decode_update_node_property_req_free);
        ASSERT_NE(nullptr, request.get());
        EXPECT_STREQ("app", request->node);
        EXPECT_STREQ("转义\"\\\n", request->property.key);
        expect_property_json(value, request->property.value_json);
        expect_property_response(&request->property, 1);
    }
}

TEST(JsonTest, NodePropertiesEmptyResponse)
{
    expect_property_response(NULL, 0);
}

TEST(JsonTest, NodePropertiesDelete)
{
    char input[] = R"({"node":"app","keys":["missing","转义\"\\\n"]})";
    neu_json_delete_node_properties_req_t *raw = NULL;
    ASSERT_EQ(0, neu_json_decode_delete_node_properties_req(input, &raw));
    std::unique_ptr<neu_json_delete_node_properties_req_t,
                    decltype(&neu_json_decode_delete_node_properties_req_free)>
        request(raw, neu_json_decode_delete_node_properties_req_free);
    ASSERT_NE(nullptr, request.get());
    EXPECT_STREQ("app", request->node);
    ASSERT_EQ(2, request->n_key);
    EXPECT_STREQ("missing", request->keys[0]);
    EXPECT_STREQ("转义\"\\\n", request->keys[1]);
}

TEST(JsonTest, NodePropertiesRejectInvalidValues)
{
    const char *invalid[] = {
        "null",
        "{}",
        R"({"a":1})",
        "[[1]]",
        "[[]]",
        "[null]",
        "[{}]",
        R"([1,"1"])",
        "[true,1]",
        "[false,0.0]",
        R"(["true",true])",
        "9223372036854775808",
        "-9223372036854775809",
        "1e309",
        "NaN",
        "Infinity",
    };
    for (const char *value : invalid) {
        SCOPED_TRACE(value);
        std::string add =
            R"({"node":"driver","properties":[{"key":"valid","value":0},{"key":"bad","value":)";
        add += value;
        add += "}]}";
        neu_json_add_node_properties_req_t *add_req = NULL;
        EXPECT_NE(0,
                  neu_json_decode_add_node_properties_req(
                      const_cast<char *>(add.c_str()), &add_req));
        EXPECT_EQ(nullptr, add_req);
        neu_json_decode_add_node_properties_req_free(add_req);

        std::string update = R"({"node":"app","key":"bad","value":)";
        update += value;
        update += "}";
        neu_json_update_node_property_req_t *update_req = NULL;
        EXPECT_NE(0,
                  neu_json_decode_update_node_property_req(
                      const_cast<char *>(update.c_str()), &update_req));
        EXPECT_EQ(nullptr, update_req);
        neu_json_decode_update_node_property_req_free(update_req);
    }
}

TEST(JsonTest, NodePropertiesRejectInvalidAdd)
{
    const char *invalid[] = {
        "",
        "{",
        "[]",
        "null",
        "{}",
        R"({"properties":[{"key":"a","value":0}]})",
        R"({"node":"","properties":[{"key":"a","value":0}]})",
        R"({"node":1,"properties":[{"key":"a","value":0}]})",
        R"({"node":null,"properties":[{"key":"a","value":0}]})",
        R"({"node":"a\u0000b","properties":[{"key":"a","value":0}]})",
        R"({"node":"driver"})",
        R"({"node":"driver","properties":[]})",
        R"({"node":"driver","properties":{}})",
        R"({"node":"driver","properties":null})",
        R"({"node":"driver","properties":[null]})",
        R"({"node":"driver","properties":[{"value":0}]})",
        R"({"node":"driver","properties":[{"key":"" ,"value":0}]})",
        R"({"node":"driver","properties":[{"key":false,"value":0}]})",
        R"({"node":"driver","properties":[{"key":"a\u0000b","value":0}]})",
        R"({"node":"driver","properties":[{"key":"a"}]})",
        R"({"node":"driver","properties":[{"key":"a","value":0},{"key":"a","value":1}]})",
        R"({"node":"driver","properties":[{"key":"a","key":"b","value":0}]})",
        R"({"node":"driver","properties":[{"key":"a","value":0,"value":1}]})",
        R"({"node":"driver","node":"app","properties":[{"key":"a","value":0}]})",
    };
    for (const char *input : invalid) {
        SCOPED_TRACE(input);
        neu_json_add_node_properties_req_t *request = NULL;
        EXPECT_NE(0,
                  neu_json_decode_add_node_properties_req(
                      const_cast<char *>(input), &request));
        EXPECT_EQ(nullptr, request);
        neu_json_decode_add_node_properties_req_free(request);
    }
}

TEST(JsonTest, NodePropertiesRejectInvalidDelete)
{
    const char *invalid[] = {
        "",
        "{",
        "[]",
        "null",
        "{}",
        R"({"keys":["a"]})",
        R"({"node":"","keys":["a"]})",
        R"({"node":null,"keys":["a"]})",
        R"({"node":1,"keys":["a"]})",
        R"({"node":"a\u0000b","keys":["a"]})",
        R"({"node":"app"})",
        R"({"node":"app","keys":[]})",
        R"({"node":"app","keys":null})",
        R"({"node":"app","keys":"a"})",
        R"({"node":"app","keys":["valid",""]})",
        R"({"node":"app","keys":["valid",null]})",
        R"({"node":"app","keys":["valid",1]})",
        R"({"node":"app","keys":["valid",[]]})",
        R"({"node":"app","keys":["valid",{}]})",
        R"({"node":"app","keys":["valid","a\u0000b"]})",
        R"({"node":"app","keys":["a","a"]})",
        R"({"node":"app","keys":["a"],"keys":["b"]})",
    };
    for (const char *input : invalid) {
        SCOPED_TRACE(input);
        neu_json_delete_node_properties_req_t *request = NULL;
        EXPECT_NE(0,
                  neu_json_decode_delete_node_properties_req(
                      const_cast<char *>(input), &request));
        EXPECT_EQ(nullptr, request);
        neu_json_decode_delete_node_properties_req_free(request);
    }
}

TEST(JsonTest, NodePropertiesRejectInvalidUpdate)
{
    const char *invalid[] = {
        "",
        "{",
        "[]",
        "null",
        "{}",
        R"({"key":"a","value":0})",
        R"({"node":"","key":"a","value":0})",
        R"({"node":null,"key":"a","value":0})",
        R"({"node":1,"key":"a","value":0})",
        R"({"node":"a\u0000b","key":"a","value":0})",
        R"({"node":"app","value":0})",
        R"({"node":"app","key":"a"})",
        R"({"node":"app","key":"","value":0})",
        R"({"node":"app","key":false,"value":0})",
        R"({"node":"app","key":"a\u0000b","value":0})",
        R"({"node":"app","key":"a","value":0,"value":1})",
    };
    for (const char *input : invalid) {
        SCOPED_TRACE(input);
        neu_json_update_node_property_req_t *request = NULL;
        EXPECT_NE(0,
                  neu_json_decode_update_node_property_req(
                      const_cast<char *>(input), &request));
        EXPECT_EQ(nullptr, request);
        neu_json_decode_update_node_property_req_free(request);
    }
}

int main(int argc, char **argv)
{
    zlog_init("./config/dev.conf");
    neuron = zlog_get_category("neuron");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}