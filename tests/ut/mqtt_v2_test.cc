#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <gtest/gtest.h>
#include <jansson.h>

#include "neuron.h"

#include "mqtt_v2/mqtt_v2_params.h"
#include "mqtt_v2/mqtt_v2_template.h"
#include "mqtt_v2/mqtt_v2_write.h"

int64_t          global_timestamp = 0;
zlog_category_t *neuron           = NULL;

#define DATA_DEFAULT                                                \
    "{\"timestamp\": \"${timestamp}\", \"device\": \"${device}\", " \
    "\"group\": \"${group}\", \"tags\": \"${tags}\"}"

#define TAG_DEFAULT                                                          \
    "{\"name\": \"${tag.name}\", \"value\": \"${tag.value}\", "              \
    "\"timestamp\": \"${tag.timestamp}\", \"quality\": \"${tag.quality}\", " \
    "\"description\": \"${tag.description}\", \"unit\": \"${tag.unit}\", "   \
    "\"error\": \"${tag.error}\"}"

class MqttV2Template : public ::testing::Test {
protected:
    void SetUp() override
    {
        utarray_new(tags, neu_resp_tag_value_meta_icd());

        neu_resp_tag_value_meta_t t1 = {};
        strcpy(t1.tag, "t1");
        t1.value.type      = NEU_TYPE_INT16;
        t1.value.value.i16 = 12;
        t1.datatag.type    = NEU_TYPE_INT16;
        utarray_push_back(tags, &t1);

        neu_resp_tag_value_meta_t t2 = {};
        strcpy(t2.tag, "t2");
        t2.value.type      = NEU_TYPE_FLOAT;
        t2.value.value.f32 = 1.5;
        t2.value.precision = 1;
        t2.n_meta          = 2;
        t2.metas = (neu_tag_meta_t *) calloc(2, sizeof(neu_tag_meta_t));
        strcpy(t2.metas[0].name, "Quality");
        t2.metas[0].value.type      = NEU_TYPE_INT32;
        t2.metas[0].value.value.i32 = 192;
        strcpy(t2.metas[1].name, "SourceTimestamp");
        t2.metas[1].value.type = NEU_TYPE_STRING;
        strcpy(t2.metas[1].value.value.str, "2026-11-16T02:03:00Z");
        utarray_push_back(tags, &t2);

        neu_resp_tag_value_meta_t t3 = {};
        strcpy(t3.tag, "terr");
        t3.value.type      = NEU_TYPE_ERROR;
        t3.value.value.i32 = NEU_ERR_PLUGIN_TAG_VALUE_EXPIRED;
        utarray_push_back(tags, &t3);

        properties = json_pack("{sssi}", "location", "china", "line", 34);
    }

    void TearDown() override
    {
        utarray_foreach(tags, neu_resp_tag_value_meta_t *, tag)
        {
            free(tag->metas);
        }
        utarray_free(tags);
        json_decref(properties);
        mqtt_v2_tmpl_free(data_tmpl);
        mqtt_v2_tmpl_free(tag_tmpl);
    }

    static bool tag_info(void *ctx, const char *tag, mqtt_v2_tag_info_t *info)
    {
        (void) ctx;
        if (0 == strcmp(tag, "t1")) {
            info->description = "temperature";
            info->unit        = "C";
            return true;
        }
        if (0 == strcmp(tag, "t2")) {
            info->description = "";
            info->unit        = NULL;
            return true;
        }
        return false;
    }

    void compile(const char *data, const char *tag)
    {
        char err[256] = { 0 };
        ASSERT_EQ(0,
                  mqtt_v2_tmpl_compile(data, MQTT_V2_TMPL_DATA, &data_tmpl, err,
                                       sizeof(err)))
            << err;
        ASSERT_EQ(0,
                  mqtt_v2_tmpl_compile(tag, MQTT_V2_TMPL_TAG, &tag_tmpl, err,
                                       sizeof(err)))
            << err;
    }

    json_t *render(bool upload_err = false)
    {
        mqtt_v2_render_ctx_t ctx = {
            .app          = "app",
            .device       = "dev",
            .group        = "grp",
            .sub_topic    = "neuron/app/dev/data",
            .timestamp    = 1700000000123,
            .properties   = properties,
            .tags         = tags,
            .upload_err   = upload_err,
            .tag_tmpl     = tag_tmpl,
            .tag_info     = tag_info,
            .tag_info_ctx = NULL,
        };

        char *text = mqtt_v2_tmpl_render(data_tmpl, &ctx);
        EXPECT_NE(nullptr, text);
        json_t *json = json_loads(text, JSON_DECODE_ANY, NULL);
        free(text);
        return json;
    }

    UT_array *      tags       = NULL;
    json_t *        properties = NULL;
    mqtt_v2_tmpl_t *data_tmpl  = NULL;
    mqtt_v2_tmpl_t *tag_tmpl   = NULL;
};

TEST_F(MqttV2Template, default_templates)
{
    compile(DATA_DEFAULT, TAG_DEFAULT);
    // the default tag template uses ${tag.error}, so failed tags are reported
    EXPECT_TRUE(mqtt_v2_tmpl_reports_error(tag_tmpl));
    json_t *json = render(true);

    json_t *expected = json_loads(
        "{\"timestamp\": 1700000000123, \"device\": \"dev\", \"group\": "
        "\"grp\", \"tags\": [{\"name\": \"t1\", \"value\": 12, "
        "\"description\": \"temperature\", \"unit\": \"C\"}, {\"name\": "
        "\"t2\", \"value\": 1.5, \"timestamp\": \"2026-11-16T02:03:00Z\", "
        "\"quality\": 192}, {\"name\": \"terr\", \"error\": 3008}]}",
        0, NULL);
    EXPECT_TRUE(json_equal(expected, json)) << json_dumps(json, 0);

    json_decref(expected);
    json_decref(json);
}

TEST_F(MqttV2Template, error_tags_follow_template)
{
    compile(DATA_DEFAULT, "{\"name\": \"${tag.name}\"}");
    EXPECT_FALSE(mqtt_v2_tmpl_reports_error(tag_tmpl));
    json_t *json = render(false);

    json_t *expected =
        json_loads("[{\"name\": \"t1\"}, {\"name\": \"t2\"}]", 0, NULL);
    EXPECT_TRUE(json_equal(expected, json_object_get(json, "tags")))
        << json_dumps(json, 0);
    EXPECT_EQ(2u, mqtt_v2_tags_count(tags, false));
    json_decref(expected);
    json_decref(json);

    mqtt_v2_tmpl_free(data_tmpl);
    mqtt_v2_tmpl_free(tag_tmpl);
    data_tmpl = NULL;
    tag_tmpl  = NULL;
    compile(DATA_DEFAULT, TAG_DEFAULT);
    EXPECT_TRUE(mqtt_v2_tmpl_reports_error(tag_tmpl));
    EXPECT_FALSE(mqtt_v2_tmpl_reports_error(data_tmpl));
    json = render(true);

    expected = json_loads(
        "[{\"name\": \"t1\", \"value\": 12, \"description\": "
        "\"temperature\", \"unit\": \"C\"}, {\"name\": \"t2\", \"value\": "
        "1.5, \"timestamp\": \"2026-11-16T02:03:00Z\", \"quality\": 192}, "
        "{\"name\": \"terr\", \"error\": 3008}]",
        0, NULL);
    EXPECT_TRUE(json_equal(expected, json_object_get(json, "tags")))
        << json_dumps(json, 0);
    EXPECT_EQ(3u, mqtt_v2_tags_count(tags, true));

    json_decref(expected);
    json_decref(json);
}

TEST_F(MqttV2Template, tags_kv)
{
    compile("{\"values\": \"${tags_kv}\"}", "\"${tag.value}\"");
    json_t *json = render();

    json_t *expected =
        json_loads("{\"values\": {\"t1\": 12, \"t2\": 1.5}}", 0, NULL);
    EXPECT_TRUE(json_equal(expected, json)) << json_dumps(json, 0);

    json_decref(expected);
    json_decref(json);
}

TEST_F(MqttV2Template, tag_index)
{
    compile("{\"first\": \"${tags[t1]}\", \"v2\": \"${tags[t2].value}\", "
            "\"q2\": \"${tags[t2].quality}\", \"none\": \"${tags[nope]}\", "
            "\"err\": \"${tags[terr].error}\"}",
            "{\"n\": \"${tag.name}\", \"t\": \"${tag.type}\"}");
    json_t *json = render();

    json_t *expected = json_loads(
        "{\"first\": {\"n\": \"t1\", \"t\": \"INT16\"}, \"v2\": 1.5, \"q2\": "
        "192}",
        0, NULL);
    EXPECT_TRUE(json_equal(expected, json)) << json_dumps(json, 0);

    json_decref(expected);
    json_decref(json);
}

TEST_F(MqttV2Template, properties)
{
    compile(
        "{\"props\": \"${properties}\", \"loc\": \"${properties[location]}\", "
        "\"none\": \"${properties[nope]}\"}",
        "{\"line\": \"${properties[line]}\", \"v\": \"${tag.value}\"}");
    json_t *json = render();

    json_t *expected = json_loads(
        "{\"props\": {\"location\": \"china\", \"line\": 34}, \"loc\": "
        "\"china\"}",
        0, NULL);
    EXPECT_TRUE(json_equal(expected, json)) << json_dumps(json, 0);
    json_decref(expected);
    json_decref(json);

    // the tag template sees them as well, a device may have none
    mqtt_v2_tmpl_free(data_tmpl);
    mqtt_v2_tmpl_free(tag_tmpl);
    data_tmpl = NULL;
    tag_tmpl  = NULL;
    compile("{\"t1\": \"${tags[t1]}\", \"props\": \"${properties}\"}",
            "{\"line\": \"${properties[line]}\", \"v\": \"${tag.value}\"}");
    json     = render();
    expected = json_loads(
        "{\"t1\": {\"line\": 34, \"v\": 12}, \"props\": {\"location\": "
        "\"china\", \"line\": 34}}",
        0, NULL);
    EXPECT_TRUE(json_equal(expected, json)) << json_dumps(json, 0);
    json_decref(expected);
    json_decref(json);

    json_decref(properties);
    properties = NULL;
    json       = render();
    expected   = json_loads("{\"t1\": {\"v\": 12}}", 0, NULL);
    EXPECT_TRUE(json_equal(expected, json)) << json_dumps(json, 0);
    json_decref(expected);
    json_decref(json);
}

TEST_F(MqttV2Template, sample_attrs)
{
    // attributes win over metadata
    neu_resp_tag_value_meta_t t4 = {};
    strcpy(t4.tag, "t4");
    t4.value.type                       = NEU_TYPE_INT32;
    t4.value.value.i32                  = 4;
    t4.value.attrs.has_quality          = true;
    t4.value.attrs.quality              = 0;
    t4.value.attrs.has_source_timestamp = true;
    t4.value.attrs.source_timestamp     = 1700000000999;
    t4.value.attrs.has_server_timestamp = true;
    t4.value.attrs.server_timestamp     = 1700000001000;
    t4.n_meta                           = 1;
    t4.metas = (neu_tag_meta_t *) calloc(1, sizeof(neu_tag_meta_t));
    strcpy(t4.metas[0].name, "quality");
    t4.metas[0].value.type      = NEU_TYPE_INT32;
    t4.metas[0].value.value.i32 = 192;
    t4.datatag.type             = NEU_TYPE_AUTO;
    t4.resolved_type            = NEU_TYPE_INT32;
    utarray_push_back(tags, &t4);

    // a sample timestamp of its own wins over the others
    neu_resp_tag_value_meta_t t5 = {};
    strcpy(t5.tag, "t5");
    t5.value.type                       = NEU_TYPE_BOOL;
    t5.value.value.boolean              = true;
    t5.value.attrs.has_timestamp        = true;
    t5.value.attrs.timestamp            = 5;
    t5.value.attrs.has_server_timestamp = true;
    t5.value.attrs.server_timestamp     = 6;
    t5.datatag.type                     = NEU_TYPE_AUTO;
    utarray_push_back(tags, &t5);

    compile("{\"t4\": \"${tags[t4]}\", \"t5\": \"${tags[t5]}\"}",
            "{\"q\": \"${tag.quality}\", \"ts\": \"${tag.timestamp}\", "
            "\"src\": \"${tag.sourceTimestamp}\", "
            "\"srv\": \"${tag.serverTimestamp}\", \"type\": \"${tag.type}\"}");
    json_t *json = render();

    json_t *expected = json_loads(
        "{\"t4\": {\"q\": 0, \"ts\": 1700000000999, \"src\": 1700000000999, "
        "\"srv\": 1700000001000, \"type\": \"INT32\"}, "
        "\"t5\": {\"ts\": 5, \"srv\": 6, \"type\": \"AUTO\"}}",
        0, NULL);
    EXPECT_TRUE(json_equal(expected, json)) << json_dumps(json, 0);

    json_decref(expected);
    json_decref(json);
}

TEST_F(MqttV2Template, interpolation_and_keys)
{
    compile("{\"id\": \"${app}:${device}-${group}\", \"${device}\": "
            "\"${tags_kv}\", \"topic\": \"${sub_topic}\", \"n\": 1, \"b\": "
            "true, \"arr\": [\"${device}\", \"${tags[nope]}\"]}",
            "{\"${tag.name}\": \"${tag.value}\", \"device\": \"${device}\"}");
    json_t *json = render();

    json_t *expected = json_loads(
        "{\"id\": \"app:dev-grp\", \"dev\": {\"t1\": {\"t1\": 12, \"device\": "
        "\"dev\"}, \"t2\": {\"t2\": 1.5, \"device\": \"dev\"}}, \"topic\": "
        "\"neuron/app/dev/data\", \"n\": 1, "
        "\"b\": true, \"arr\": [\"dev\", null]}",
        0, NULL);
    EXPECT_TRUE(json_equal(expected, json)) << json_dumps(json, 0);

    json_decref(expected);
    json_decref(json);
}

TEST_F(MqttV2Template, bare_variables)
{
    compile("{\"device\": ${device}, \"tags\": ${tags}, \"s\": \"x}${group}\"}",
            "{\"property\": ${tag.name}, \"value\": \"${tag.value}\"}");
    json_t *json = render(false);

    json_t *expected = json_loads(
        "{\"device\": \"dev\", \"tags\": [{\"property\": \"t1\", \"value\": "
        "12}, {\"property\": \"t2\", \"value\": 1.5}], \"s\": \"x}grp\"}",
        0, NULL);
    EXPECT_TRUE(json_equal(expected, json)) << json_dumps(json, 0);

    json_decref(expected);
    json_decref(json);
}

TEST_F(MqttV2Template, time_formats)
{
    setenv("TZ", "UTC", 1);
    tzset();

    compile(
        "{\"utc\": \"${TIME_UTC}\", \"rfc\": \"${TIME_RFC3339}\", "
        "\"local\": \"${TIME_LOCAL}\", \"unix\": \"${TIME_UNIX}\", \"fmt\": "
        "\"${TIME:%Y%m%d}\", \"ts\": \"${ timestamp }\"}",
        TAG_DEFAULT);
    json_t *json = render();

    json_t *expected = json_loads(
        "{\"utc\": \"2023-11-14T22:13:20.123Z\", \"rfc\": "
        "\"2023-11-14T22:13:20.123+00:00\", \"local\": \"2023-11-14 "
        "22:13:20.123\", \"unix\": 1700000000, \"fmt\": \"20231114\", \"ts\": "
        "1700000000123}",
        0, NULL);
    EXPECT_TRUE(json_equal(expected, json)) << json_dumps(json, 0);

    json_decref(expected);
    json_decref(json);
}

TEST_F(MqttV2Template, time_functions_take_a_timestamp)
{
    setenv("TZ", "UTC", 1);
    tzset();

    neu_resp_tag_value_meta_t *t1 =
        (neu_resp_tag_value_meta_t *) utarray_front(tags);
    t1->value.attrs.has_timestamp        = true;
    t1->value.attrs.timestamp            = 1700000001123;
    t1->value.attrs.has_source_timestamp = true;
    t1->value.attrs.source_timestamp     = 1700000002123;

    compile("{\"group_local\": \"${TIME_LOCAL(timestamp)}\", "
            "\"plain\": \"${TIME_LOCAL}\", "
            "\"one\": \"${TIME_UTC(tags[t1].timestamp)}\", "
            "\"tags\": \"${tags}\"}",
            "{\"name\": \"${tag.name}\", "
            "\"local\": \"${TIME_LOCAL( tag.timestamp )}\", "
            "\"src\": \"${TIME_UTC(tag.sourceTimestamp)}\", "
            "\"srv\": \"${TIME_RFC3339(tag.serverTimestamp)}\", "
            "\"unix\": \"${TIME_UNIX(tag.timestamp)}\", "
            "\"fmt\": \"${TIME:%H%M%S(tag.timestamp)}\"}");
    json_t *json = render();

    json_t *expected = json_loads(
        "{\"group_local\": \"2023-11-14 22:13:20.123\", "
        "\"plain\": \"2023-11-14 22:13:20.123\", "
        "\"one\": \"2023-11-14T22:13:21.123Z\", "
        "\"tags\": ["
        "{\"name\": \"t1\", \"local\": \"2023-11-14 22:13:21.123\", "
        "\"src\": \"2023-11-14T22:13:22.123Z\", \"unix\": 1700000001, "
        "\"fmt\": \"221321\"}, "
        "{\"name\": \"t2\"}]}",
        0, NULL);
    EXPECT_TRUE(json_equal(expected, json)) << json_dumps(json, 0);

    json_decref(expected);
    json_decref(json);
}

TEST(MqttV2TemplateCompile, errors)
{
    struct {
        const char *        text;
        mqtt_v2_tmpl_kind_e kind;
        const char *        reason;
    } cases[] = {
        { "{\"a\": \"${nope}\"}", MQTT_V2_TMPL_DATA, "unknown variable" },
        { "{\"a\": \"${tag.value}\"}", MQTT_V2_TMPL_DATA,
          "only be used in the tag template" },
        { "{\"a\": \"${tags}\"}", MQTT_V2_TMPL_TAG, "cannot be used" },
        { "{\"a\": \"${tags[x]}\"}", MQTT_V2_TMPL_TAG, "cannot be used" },
        { "{\"a\": \"${tag.nope}\"}", MQTT_V2_TMPL_TAG,
          "unknown tag attribute" },
        { "{\"a\": \"${tags[x].nope}\"}", MQTT_V2_TMPL_DATA,
          "unknown tag attribute" },
        { "{\"a\": \"${tags[]}\"}", MQTT_V2_TMPL_DATA,
          "invalid tag reference" },
        { "{\"a\": \"${properties[]}\"}", MQTT_V2_TMPL_DATA,
          "empty property key" },
        { "{\"a\": \"${TIME:}\"}", MQTT_V2_TMPL_DATA, "invalid time format" },
        { "{\"a\": \"${TIME_LOCAL()}\"}", MQTT_V2_TMPL_DATA,
          "empty time argument" },
        { "{\"a\": \"${TIME_LOCAL(tag.value)}\"}", MQTT_V2_TMPL_TAG,
          "not a timestamp" },
        { "{\"a\": \"${TIME_LOCAL(nope)}\"}", MQTT_V2_TMPL_DATA,
          "unknown time argument" },
        { "{\"a\": \"${TIME_LOCAL(tag.timestamp)}\"}", MQTT_V2_TMPL_DATA,
          "only be used in the tag template" },
        { "{\"a\": \"${TIME_LOCAL(tags[x].timestamp)}\"}", MQTT_V2_TMPL_TAG,
          "cannot be used" },
        { "{\"a\": \"${device\"}", MQTT_V2_TMPL_DATA, "unterminated" },
        { "{\"a\": ", MQTT_V2_TMPL_DATA, "invalid JSON" },
    };

    for (auto &c : cases) {
        mqtt_v2_tmpl_t *tmpl     = NULL;
        char            err[256] = { 0 };
        EXPECT_EQ(-1,
                  mqtt_v2_tmpl_compile(c.text, c.kind, &tmpl, err, sizeof(err)))
            << c.text;
        EXPECT_NE(nullptr, strstr(err, c.reason)) << c.text << ": " << err;
        EXPECT_EQ(nullptr, tmpl);
    }
}

TEST(MqttV2Topic, render)
{
    char *topic = mqtt_v2_topic_render("a/${app}/${device}/${group}/x", NULL,
                                       "app", "d+1#", "g");
    EXPECT_STREQ("a/app/d_1_/g/x", topic);
    free(topic);

    topic =
        mqtt_v2_topic_render("neuron/${app}", NULL, "x${device}", NULL, NULL);
    EXPECT_STREQ("neuron/x${device}", topic);
    free(topic);

    // the default data topic
    topic = mqtt_v2_topic_render("${prefix}/${device}/data", "neuron/app",
                                 "app", "dev", "g");
    EXPECT_STREQ("neuron/app/dev/data", topic);
    free(topic);

    // until the prefix is configured
    topic =
        mqtt_v2_topic_render("${prefix}/${device}", NULL, "app", "dev", "g");
    EXPECT_STREQ("/dev", topic);
    free(topic);
}

TEST(MqttV2Params, topic)
{
    static const char *prefix_vars[] = { "app", NULL };

    EXPECT_TRUE(mqtt_v2_topic_tmpl_valid("neuron/${app}", prefix_vars));
    EXPECT_FALSE(mqtt_v2_topic_tmpl_valid("neuron/${device}", prefix_vars));
    EXPECT_TRUE(mqtt_v2_sub_topic_valid("plant/${app}/${device}/${group}"));
    EXPECT_TRUE(mqtt_v2_sub_topic_valid("${device}/data"));
    EXPECT_TRUE(mqtt_v2_sub_topic_valid("${prefix}/${device}/data"));
    EXPECT_FALSE(mqtt_v2_topic_tmpl_valid("${prefix}/x", prefix_vars));
    EXPECT_FALSE(mqtt_v2_sub_topic_valid(""));
    EXPECT_FALSE(mqtt_v2_sub_topic_valid("a/+/b"));
    EXPECT_FALSE(mqtt_v2_sub_topic_valid("a/#"));
    EXPECT_FALSE(mqtt_v2_sub_topic_valid("$SYS/a"));
    EXPECT_FALSE(mqtt_v2_sub_topic_valid("a/${nope}"));
    EXPECT_FALSE(mqtt_v2_sub_topic_valid("a/${device"));
}

TEST(MqttV2Params, check)
{
    EXPECT_EQ(NEU_ERR_SUCCESS, mqtt_v2_params_check(NULL));
    EXPECT_EQ(NEU_ERR_SUCCESS, mqtt_v2_params_check("{\"params\": {}}"));
    EXPECT_EQ(
        NEU_ERR_SUCCESS,
        mqtt_v2_params_check("{\"params\": {\"topic\": \"a/${device}\"}}"));
    // the default topic
    EXPECT_EQ(NEU_ERR_SUCCESS,
              mqtt_v2_params_check("{\"params\": {\"topic\": \"\"}}"));
    EXPECT_EQ(NEU_ERR_SUCCESS,
              mqtt_v2_params_check("{\"params\": {\"topic\": null}}"));
    // unknown fields are ignored
    EXPECT_EQ(NEU_ERR_SUCCESS,
              mqtt_v2_params_check("{\"params\": {\"other\": [1]}}"));
    EXPECT_EQ(NEU_ERR_GROUP_PARAMETER_INVALID, mqtt_v2_params_check("nope"));
    EXPECT_EQ(NEU_ERR_GROUP_PARAMETER_INVALID,
              mqtt_v2_params_check("{\"params\": \"x\"}"));
    EXPECT_EQ(NEU_ERR_GROUP_PARAMETER_INVALID,
              mqtt_v2_params_check("{\"params\": {\"topic\": \"a/#\"}}"));
    EXPECT_EQ(NEU_ERR_GROUP_PARAMETER_INVALID,
              mqtt_v2_params_check("{\"params\": {\"topic\": 1}}"));
}

static int decode(const char *text, mqtt_v2_write_req_t *req)
{
    return mqtt_v2_write_req_decode((const uint8_t *) text, strlen(text), req);
}

TEST(MqttV2Write, decode_neuron)
{
    const char *versions[] = {
        "{\"id\": \"123\", \"params\": {\"values\": {\"brightness\": 40, "
        "\"power\": true, \"ratio\": 0.5, \"name\": \"x\", \"arr\": [1, 2]}}}",
        "{\"id\": \"123\", \"version\": \"1.0\", \"params\": {\"values\": "
        "{\"brightness\": 40, \"power\": true, \"ratio\": 0.5, \"name\": "
        "\"x\", "
        "\"arr\": [1, 2]}}}",
        "{\"id\": \"123\", \"version\": \"neuron\", \"params\": {\"values\": "
        "{\"brightness\": 40, \"power\": true, \"ratio\": 0.5, \"name\": "
        "\"x\", "
        "\"arr\": [1, 2]}}}",
    };

    for (const char *text : versions) {
        mqtt_v2_write_req_t req = {};
        ASSERT_EQ(NEU_ERR_SUCCESS, decode(text, &req)) << text;
        EXPECT_STREQ("123", json_string_value(req.id));
        EXPECT_EQ(nullptr, req.group);
        ASSERT_EQ(5u, req.n_value);

        EXPECT_STREQ("brightness", req.values[0].tag);
        EXPECT_EQ(NEU_TYPE_INT64, req.values[0].value.type);
        EXPECT_EQ(40, req.values[0].value.value.i64);
        EXPECT_EQ(NEU_TYPE_BOOL, req.values[1].value.type);
        EXPECT_TRUE(req.values[1].value.value.boolean);
        EXPECT_EQ(NEU_TYPE_DOUBLE, req.values[2].value.type);
        EXPECT_DOUBLE_EQ(0.5, req.values[2].value.value.d64);
        EXPECT_EQ(NEU_TYPE_STRING, req.values[3].value.type);
        EXPECT_STREQ("x", req.values[3].value.value.str);
        EXPECT_EQ(NEU_TYPE_ARRAY_INT64, req.values[4].value.type);
        EXPECT_EQ(2u, req.values[4].value.value.i64s.length);
        EXPECT_EQ(2, req.values[4].value.value.i64s.i64s[1]);

        mqtt_v2_write_req_fini(&req);
    }
}

TEST(MqttV2Write, decode_errors)
{
    mqtt_v2_write_req_t req = {};

    EXPECT_EQ(NEU_ERR_BODY_IS_WRONG, decode("not json", &req));
    mqtt_v2_write_req_fini(&req);

    EXPECT_EQ(NEU_ERR_BODY_IS_WRONG,
              decode("{\"id\": 1, \"params\": {\"values\": {}}}", &req));
    EXPECT_EQ(1, json_integer_value(req.id));
    mqtt_v2_write_req_fini(&req);

    EXPECT_EQ(NEU_ERR_PARAM_IS_WRONG,
              decode("{\"id\": \"a\", \"version\": \"2.0\", \"params\": "
                     "{\"values\": {\"t\": 1}}}",
                     &req));
    EXPECT_STREQ("2.0", req.version);
    mqtt_v2_write_req_fini(&req);

    EXPECT_EQ(NEU_ERR_PARAM_IS_WRONG,
              decode("{\"params\": {\"values\": {\"ok\": 1, \"bad\": null, "
                     "\"mix\": [1, \"a\"]}}}",
                     &req));
    ASSERT_EQ(3u, req.n_value);
    EXPECT_EQ(NEU_ERR_SUCCESS, req.values[0].error);
    EXPECT_EQ(NEU_ERR_PARAM_IS_WRONG, req.values[1].error);
    EXPECT_EQ(NEU_ERR_PARAM_IS_WRONG, req.values[2].error);
    mqtt_v2_write_values_fail(req.values, req.n_value, NEU_ERR_PARAM_IS_WRONG);
    EXPECT_EQ(NEU_ERR_PARAM_IS_WRONG, req.values[0].error);
    mqtt_v2_write_req_fini(&req);

    EXPECT_EQ(
        NEU_ERR_SUCCESS,
        decode("{\"params\": {\"group\": \"g\", \"values\": {\"t\": [\"a\", "
               "\"b\"]}}}",
               &req));
    EXPECT_STREQ("g", req.group);
    EXPECT_EQ(NEU_TYPE_ARRAY_STRING, req.values[0].value.type);
    mqtt_v2_write_req_fini(&req);
}

TEST(MqttV2Write, response)
{
    mqtt_v2_write_req_t req = {};
    ASSERT_EQ(NEU_ERR_SUCCESS,
              decode("{\"id\": \"123\", \"version\": \"1.0\", \"params\": "
                     "{\"values\": {\"a\": 1, \"b\": 2}}}",
                     &req));
    req.values[1].error = NEU_ERR_TAG_NOT_EXIST;

    char *text = mqtt_v2_write_resp_encode(
        req.id, req.version, NEU_ERR_TAG_NOT_EXIST, req.values, req.n_value);
    json_t *json     = json_loads(text, 0, NULL);
    json_t *expected = json_loads(
        "{\"id\": \"123\", \"version\": \"1.0\", \"error\": 2201, \"tags\": "
        "{\"a\": 0, \"b\": 2201}}",
        0, NULL);
    EXPECT_TRUE(json_equal(expected, json)) << text;

    free(text);
    json_decref(json);
    json_decref(expected);
    mqtt_v2_write_req_fini(&req);
}
