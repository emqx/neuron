#include <gtest/gtest.h>
#include <limits>

extern "C" {
#include "errcodes.h"
#include "type.h"
int  is_value_in_range(neu_type_e, int64_t, double, neu_type_e, double);
int  auto_test_prepare(neu_type_e, neu_dvalue_t *, double, double, const char *,
                       int, bool *, neu_type_e *);
void auto_test_read(neu_dvalue_t, double, const char *, int, neu_dvalue_t *,
                    neu_type_e *);
int  auto_test_metadata(neu_type_e, bool, int *, neu_type_e *);
}

TEST(DriverAuto, UnresolvedWrite)
{
    neu_dvalue_t value = {};
    value.type         = NEU_TYPE_INT64;
    value.value.i64    = 123;
    bool       valid;
    neu_type_e type;
    EXPECT_EQ(NEU_ERR_PLUGIN_TAG_NOT_READY,
              auto_test_prepare(NEU_TYPE_AUTO, &value, 0, 0, "sample", 0,
                                &valid, &type));
}

TEST(DriverAuto, ConcreteWriteAndEndian)
{
    for (const char *address : { "sample", "sample#B" }) {
        neu_dvalue_t value = {};
        value.type         = NEU_TYPE_INT64;
        value.value.i64    = 123;
        bool       valid;
        neu_type_e type;
        EXPECT_EQ(0,
                  auto_test_prepare(NEU_TYPE_INT16, &value, 0, 0, address, 0,
                                    &valid, &type));
        EXPECT_EQ(NEU_TYPE_INT16, type);
        EXPECT_EQ(NEU_TYPE_INT16, value.type);
        EXPECT_EQ(address[6] ? htons(123) : 123, value.value.u16);
        EXPECT_TRUE(valid);
    }
}

TEST(DriverAuto, RangeAndRealInputType)
{
    for (neu_type_e input : { NEU_TYPE_INT64, NEU_TYPE_STRING }) {
        neu_dvalue_t value = {};
        value.type         = input;
        value.value.i64    = 40000;
        bool       valid;
        neu_type_e type;
        EXPECT_EQ(input == NEU_TYPE_INT64
                      ? NEU_ERR_PLUGIN_TAG_VALUE_OUT_OF_RANGE
                      : NEU_ERR_PLUGIN_TAG_TYPE_MISMATCH,
                  auto_test_prepare(NEU_TYPE_INT16, &value, 0, 0, "sample", 0,
                                    &valid, &type));
    }
}

TEST(DriverAuto, PendingConfigAndGenerationInvalidation)
{
    for (int invalidate : { 1, 2, 3 }) {
        neu_dvalue_t value = {};
        value.type         = NEU_TYPE_INT64;
        value.value.i64    = 123;
        bool       valid;
        neu_type_e type;
        EXPECT_EQ(0,
                  auto_test_prepare(NEU_TYPE_INT16, &value, 0, 0, "sample",
                                    invalidate, &valid, &type));
        EXPECT_FALSE(valid);
    }
}

TEST(DriverAuto, ScaledSignedWrite)
{
    neu_dvalue_t value = {};
    value.type         = NEU_TYPE_DOUBLE;
    value.value.d64    = -123.5;
    bool       valid;
    neu_type_e type;
    EXPECT_EQ(0,
              auto_test_prepare(NEU_TYPE_INT16, &value, 0.5, 0, "sample", 0,
                                &valid, &type));
    EXPECT_EQ(-247, value.value.i16);
    EXPECT_TRUE(valid);
}

TEST(DriverAuto, BiasAndScaleWriteInverse)
{
    for (double decimal : { 0.0, 0.5 }) {
        neu_dvalue_t value = {};
        value.type         = NEU_TYPE_INT64;
        value.value.i64    = 51;
        bool       valid;
        neu_type_e type;
        EXPECT_EQ(0,
                  auto_test_prepare(NEU_TYPE_INT16, &value, decimal, 1,
                                    "sample", 0, &valid, &type));
        EXPECT_EQ(decimal == 0 ? 50 : 100, value.value.i16);
        EXPECT_TRUE(valid);
    }
}

TEST(DriverAuto, WideIntegerBiasPreservesPrecision)
{
    for (int64_t input : { INT64_C(9007199254740994), INT64_MAX }) {
        neu_dvalue_t value = {};
        value.type         = NEU_TYPE_INT64;
        value.value.i64    = input;
        bool       valid;
        neu_type_e type;
        EXPECT_EQ(0,
                  auto_test_prepare(NEU_TYPE_INT64, &value, 0, 1, "sample", 0,
                                    &valid, &type));
        EXPECT_EQ(input - 1, value.value.i64);
        EXPECT_TRUE(valid);
    }
}

TEST(DriverAuto, FloatBiasPreservesIntegerPrecision)
{
    for (neu_type_e target : { NEU_TYPE_FLOAT, NEU_TYPE_DOUBLE }) {
        neu_dvalue_t value = {};
        value.type         = NEU_TYPE_INT64;
        value.value.i64    = INT64_C(9007199254740993);
        bool       valid;
        neu_type_e type;
        EXPECT_EQ(0,
                  auto_test_prepare(target, &value, 0, 9007199254740992.0,
                                    "sample", 0, &valid, &type));
        if (target == NEU_TYPE_FLOAT) {
            EXPECT_FLOAT_EQ(1, value.value.f32);
        } else {
            EXPECT_DOUBLE_EQ(1, value.value.d64);
        }
        EXPECT_TRUE(valid);
    }
}

TEST(DriverAuto, ValueMetadataMustBeConcreteOrNotReady)
{
    int        count;
    neu_type_e type;
    EXPECT_EQ(NEU_ERR_PLUGIN_TAG_NOT_READY,
              auto_test_metadata(NEU_TYPE_AUTO, false, &count, &type));
    EXPECT_EQ(0, count);
    EXPECT_EQ(0, auto_test_metadata(NEU_TYPE_INT16, false, &count, &type));
    EXPECT_EQ(1, count);
    EXPECT_EQ(NEU_TYPE_INT16, type);
    EXPECT_EQ(NEU_ERR_PLUGIN_TAG_NOT_READY,
              auto_test_metadata(NEU_TYPE_INT16, true, &count, &type));
    EXPECT_EQ(0, count);
}

TEST(DriverAuto, FloatWriteUsesNumericUnion)
{
    neu_dvalue_t value = {};
    value.type         = NEU_TYPE_INT64;
    value.value.i64    = 123;
    bool       valid;
    neu_type_e type;
    EXPECT_EQ(0,
              auto_test_prepare(NEU_TYPE_FLOAT, &value, 0.5, 0, "sample", 0,
                                &valid, &type));
    EXPECT_FLOAT_EQ(246, value.value.f32);
}

TEST(DriverAuto, NonfiniteAndWideBounds)
{
    for (double value : { std::numeric_limits<double>::infinity(),
                          -std::numeric_limits<double>::infinity(),
                          std::numeric_limits<double>::quiet_NaN() }) {
        EXPECT_EQ(
            NEU_ERR_PLUGIN_TAG_VALUE_OUT_OF_RANGE,
            is_value_in_range(NEU_TYPE_FLOAT, 0, value, NEU_TYPE_DOUBLE, 0));
        EXPECT_EQ(
            NEU_ERR_PLUGIN_TAG_VALUE_OUT_OF_RANGE,
            is_value_in_range(NEU_TYPE_INT64, 0, value, NEU_TYPE_DOUBLE, 0.5));
    }
    EXPECT_EQ(
        NEU_ERR_PLUGIN_TAG_VALUE_OUT_OF_RANGE,
        is_value_in_range(NEU_TYPE_INT64, INT64_MAX, 0, NEU_TYPE_INT64, 0.5));
    EXPECT_EQ(NEU_ERR_PLUGIN_TAG_VALUE_OUT_OF_RANGE,
              is_value_in_range(NEU_TYPE_UINT64, -1, 0, NEU_TYPE_INT64, 0));
    EXPECT_EQ(0, is_value_in_range(NEU_TYPE_UINT64, -1, 0, NEU_TYPE_UINT64, 0));
    EXPECT_EQ(0, is_value_in_range(NEU_TYPE_FLOAT, -1, 0, NEU_TYPE_UINT64, 0));
    EXPECT_EQ(0,
              is_value_in_range(NEU_TYPE_FLOAT, 0, 1e30, NEU_TYPE_DOUBLE, 0));
    EXPECT_EQ(NEU_ERR_PLUGIN_TAG_VALUE_OUT_OF_RANGE,
              is_value_in_range(NEU_TYPE_FLOAT, 0, 1e40, NEU_TYPE_DOUBLE, 0));
}

static neu_tag_sample_attrs_t sample_attrs()
{
    neu_tag_sample_attrs_t attrs = {};
    attrs.has_timestamp          = true;
    attrs.timestamp              = INT64_C(1700000000000);
    attrs.has_source_timestamp   = true;
    attrs.source_timestamp       = INT64_C(1700000000123);
    attrs.has_server_timestamp   = true;
    attrs.server_timestamp       = INT64_C(1700000000456);
    attrs.has_quality            = true;
    attrs.quality                = 192;
    return attrs;
}

static void expect_attrs(const neu_tag_sample_attrs_t &expected,
                         const neu_tag_sample_attrs_t &actual)
{
    EXPECT_EQ(expected.has_timestamp, actual.has_timestamp);
    EXPECT_EQ(expected.has_source_timestamp, actual.has_source_timestamp);
    EXPECT_EQ(expected.has_server_timestamp, actual.has_server_timestamp);
    EXPECT_EQ(expected.has_quality, actual.has_quality);
    EXPECT_EQ(expected.has_timestamp ? expected.timestamp : 0,
              actual.timestamp);
    EXPECT_EQ(expected.has_source_timestamp ? expected.source_timestamp : 0,
              actual.source_timestamp);
    EXPECT_EQ(expected.has_server_timestamp ? expected.server_timestamp : 0,
              actual.server_timestamp);
    EXPECT_EQ(expected.has_quality ? expected.quality : 0, actual.quality);
}

TEST(DriverAuto, ReadPathsScaleWithoutChangingConfig)
{
    for (int path : { 0, 1, 2 }) {
        neu_dvalue_t sample = {};
        sample.type         = NEU_TYPE_INT16;
        sample.value.i16    = -246;
        sample.attrs        = sample_attrs();
        neu_dvalue_t output = {};
        neu_type_e   config;
        auto_test_read(sample, 0.5, "sample", path, &output, &config);
        EXPECT_EQ(NEU_TYPE_AUTO, config);
        EXPECT_EQ(NEU_TYPE_DOUBLE, output.type);
        EXPECT_DOUBLE_EQ(-123, output.value.d64);
        expect_attrs(sample.attrs, output.attrs);
    }
}

TEST(DriverAuto, ReadPathsResolveEndian)
{
    for (int path : { 0, 1, 2 }) {
        neu_dvalue_t sample = {};
        sample.type         = NEU_TYPE_INT16;
        sample.value.u16    = htons(123);
        sample.attrs        = sample_attrs();
        neu_dvalue_t output = {};
        neu_type_e   config;
        auto_test_read(sample, 0, "sample#B", path, &output, &config);
        EXPECT_EQ(NEU_TYPE_AUTO, config);
        EXPECT_EQ(NEU_TYPE_INT16, output.type);
        EXPECT_EQ(123, output.value.i16);
        expect_attrs(sample.attrs, output.attrs);
    }
}

TEST(DriverAuto, ReadPathsPreserveOptionalTimestamps)
{
    for (int path : { 0, 1, 2 }) {
        for (neu_type_e type :
             { NEU_TYPE_INT16, NEU_TYPE_PTR, NEU_TYPE_ERROR }) {
            for (int flags = 0; flags < 8; ++flags) {
                SCOPED_TRACE(::testing::Message()
                             << "path=" << path << " type=" << type
                             << " flags=" << flags);
                char         text[]               = "hello";
                neu_dvalue_t sample               = {};
                sample.type                       = type;
                sample.attrs                      = sample_attrs();
                sample.attrs.has_timestamp        = flags & 1;
                sample.attrs.has_source_timestamp = flags & 2;
                sample.attrs.has_server_timestamp = flags & 4;
                sample.attrs.source_timestamp     = 0;
                if (type == NEU_TYPE_PTR) {
                    sample.value.ptr.type   = NEU_TYPE_STRING;
                    sample.value.ptr.length = sizeof(text);
                    sample.value.ptr.ptr    = (uint8_t *) text;
                } else if (type == NEU_TYPE_ERROR) {
                    sample.value.i32 = NEU_ERR_PLUGIN_TAG_NOT_READY;
                } else {
                    sample.value.i16 = 123;
                }
                neu_dvalue_t output = {};
                neu_type_e   config;
                auto_test_read(sample, 0, "sample", path, &output, &config);
                EXPECT_EQ(NEU_TYPE_AUTO, config);
                EXPECT_EQ(type, output.type);
                expect_attrs(sample.attrs, output.attrs);
                if (output.type == NEU_TYPE_PTR) {
                    EXPECT_NE(sample.value.ptr.ptr, output.value.ptr.ptr);
                    EXPECT_STREQ(text, (char *) output.value.ptr.ptr);
                    free(output.value.ptr.ptr);
                } else if (output.type == NEU_TYPE_ERROR) {
                    EXPECT_EQ(sample.value.i32, output.value.i32);
                } else {
                    EXPECT_EQ(123, output.value.i16);
                }
            }
        }
    }
}
