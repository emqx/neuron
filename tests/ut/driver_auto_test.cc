#include <gtest/gtest.h>
#include <limits>

#include <condition_variable>
#include <mutex>
#include <thread>

extern "C" {
#include "adapter/driver/cache.h"
#include "base/group.h"
#include "driver_auto_helpers.h"
#include "errcodes.h"
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

TEST(DriverAuto, QueryAndReadUnresolvedOrMissingCache)
{
    for (bool paginate : { false, true }) {
        for (bool missing : { false, true }) {
            for (neu_type_e config : { NEU_TYPE_AUTO, NEU_TYPE_INT16 }) {
                auto_test_snapshot_case_t input = {};
                input.configured_type           = config;
                input.sample.type               = NEU_TYPE_ERROR;
                input.sample.value.i32          = NEU_ERR_PLUGIN_TAG_NOT_READY;
                input.missing_cache             = missing;
                auto_test_snapshot_result_t output = {};
                auto_test_snapshot(&input, paginate, &output);
                EXPECT_EQ(config, output.configured_type);
                EXPECT_EQ(config, output.listed_type);
                EXPECT_EQ(config, output.read_type);
                EXPECT_EQ(1u, output.read_count);
                EXPECT_EQ(NEU_TYPE_ERROR, output.value.type);
            }
        }
    }
}

TEST(DriverAuto, QueryAndReadRetainResolvedTypeOnError)
{
    for (bool paginate : { false, true }) {
        for (bool mismatch : { false, true }) {
            auto_test_snapshot_case_t input = {};
            input.learned_type              = NEU_TYPE_INT16;
            input.sample.type = mismatch ? NEU_TYPE_DOUBLE : NEU_TYPE_ERROR;
            if (mismatch) {
                input.sample.value.d64 = 1.5;
            } else {
                input.sample.value.i32 = NEU_ERR_PLUGIN_READ_FAILURE;
            }
            auto_test_snapshot_result_t output = {};
            auto_test_snapshot(&input, paginate, &output);
            EXPECT_EQ(NEU_TYPE_AUTO, output.configured_type);
            EXPECT_EQ(NEU_TYPE_INT16, output.listed_type);
            EXPECT_EQ(NEU_TYPE_INT16, output.read_type);
            EXPECT_EQ(NEU_TYPE_ERROR, output.value.type);
            EXPECT_EQ(mismatch ? NEU_ERR_PLUGIN_TAG_TYPE_MISMATCH
                               : NEU_ERR_PLUGIN_READ_FAILURE,
                      output.value.value.i32);
        }
    }
}

TEST(DriverAuto, QueryAndReadPendingAndExpiredTypes)
{
    for (bool paginate : { false, true }) {
        for (bool pending : { false, true }) {
            auto_test_snapshot_case_t input    = {};
            input.learned_type                 = NEU_TYPE_INT16;
            input.sample.type                  = NEU_TYPE_INT16;
            input.sample.value.i16             = 123;
            input.pending                      = pending;
            input.expired                      = true;
            auto_test_snapshot_result_t output = {};
            auto_test_snapshot(&input, paginate, &output);
            EXPECT_EQ(pending ? NEU_TYPE_AUTO : NEU_TYPE_INT16,
                      output.listed_type);
            EXPECT_EQ(pending ? NEU_TYPE_AUTO : NEU_TYPE_INT16,
                      output.read_type);
            EXPECT_EQ(NEU_TYPE_ERROR, output.value.type);
            EXPECT_EQ(NEU_ERR_PLUGIN_TAG_VALUE_EXPIRED, output.value.value.i32);
        }
    }
}

TEST(DriverAuto, QueryAndReadSemanticPointerTypes)
{
    for (bool paginate : { false, true }) {
        for (neu_type_e type : { NEU_TYPE_STRING, NEU_TYPE_BYTES }) {
            char                      text[] = "hello";
            auto_test_snapshot_case_t input  = {};
            input.sample.type                = NEU_TYPE_PTR;
            input.sample.value.ptr.type      = type;
            input.sample.value.ptr.length    = sizeof(text);
            input.sample.value.ptr.ptr = reinterpret_cast<uint8_t *>(text);
            auto_test_snapshot_result_t output = {};
            auto_test_snapshot(&input, paginate, &output);
            EXPECT_EQ(type, output.listed_type);
            EXPECT_EQ(type, output.read_type);
            ASSERT_EQ(NEU_TYPE_PTR, output.value.type);
            EXPECT_NE(input.sample.value.ptr.ptr, output.value.value.ptr.ptr);
            EXPECT_EQ(0,
                      memcmp(text, output.value.value.ptr.ptr, sizeof(text)));
            free(output.value.value.ptr.ptr);
        }
    }
}

TEST(DriverAuto, SyntheticReadErrorsKeepKnownTypes)
{
    for (bool paginate : { false, true }) {
        for (neu_type_e config : { NEU_TYPE_AUTO, NEU_TYPE_INT16 }) {
            for (int error : { NEU_ERR_PLUGIN_NOT_RUNNING,
                               NEU_ERR_PLUGIN_NOT_SUPPORT_READ_SYNC }) {
                for (bool pending : { false, true }) {
                    auto_test_snapshot_case_t input = {};
                    input.configured_type           = config;
                    input.learned_type              = NEU_TYPE_INT16;
                    input.sample.type               = NEU_TYPE_ERROR;
                    input.sample.value.i32 = NEU_ERR_PLUGIN_READ_FAILURE;
                    input.pending          = pending;
                    input.error            = error;
                    auto_test_snapshot_result_t output = {};
                    auto_test_snapshot(&input, paginate, &output);
                    neu_type_e expected = config == NEU_TYPE_AUTO && pending
                        ? NEU_TYPE_AUTO
                        : NEU_TYPE_INT16;
                    EXPECT_EQ(expected, output.listed_type);
                    EXPECT_EQ(expected, output.read_type);
                    EXPECT_EQ(NEU_TYPE_ERROR, output.value.type);
                    EXPECT_EQ(error, output.value.value.i32);
                }
            }
        }
    }
}

TEST(DriverAuto, WriteOnlyPaginationUsesConfiguredType)
{
    for (bool paginate : { false, true }) {
        auto_test_snapshot_case_t input    = {};
        input.configured_type              = NEU_TYPE_INT32;
        input.write_only                   = true;
        input.missing_cache                = true;
        auto_test_snapshot_result_t output = {};
        auto_test_snapshot(&input, paginate, &output);
        EXPECT_EQ(NEU_TYPE_INT32, output.listed_type);
        EXPECT_EQ(paginate ? 1u : 0u, output.read_count);
        if (paginate) {
            EXPECT_EQ(NEU_TYPE_INT32, output.read_type);
            EXPECT_EQ(NEU_TYPE_STRING, output.value.type);
        }
    }
}

TEST(DriverAuto, GroupSnapshotDoesNotObservePartialRebuild)
{
    struct Context {
        neu_driver_cache_t *    cache;
        int64_t                 applied = 0;
        std::mutex              mutex;
        std::condition_variable changed;
        bool                    rebuilding        = false;
        bool                    query_started     = false;
        bool                    release           = false;
        bool                    complete          = false;
        bool                    observed_complete = false;
        neu_type_e              first             = NEU_TYPE_AUTO;
        neu_type_e              second            = NEU_TYPE_AUTO;
    } ctx;
    ctx.cache           = neu_driver_cache_new();
    neu_group_t * group = neu_group_new("g", 100);
    neu_datatag_t tag   = {};
    tag.name            = const_cast<char *>("a");
    tag.address         = const_cast<char *>("sample");
    tag.type            = NEU_TYPE_AUTO;
    tag.attribute       = NEU_ATTRIBUTE_READ;
    ASSERT_EQ(0, neu_group_add_tag(group, &tag));
    neu_dvalue_t initial = {};
    initial.type         = NEU_TYPE_ERROR;
    initial.value.i32    = NEU_ERR_PLUGIN_TAG_NOT_READY;
    neu_driver_cache_add_tag(ctx.cache, "g", &tag, initial);
    neu_dvalue_t sample = {};
    sample.type         = NEU_TYPE_INT16;
    sample.value.i16    = 123;
    neu_driver_cache_update(ctx.cache, "g", "a", 100, sample, nullptr, 0);
    tag.name = const_cast<char *>("b");
    ASSERT_EQ(0, neu_group_add_tag(group, &tag));

    auto inspect = [](void *arg, int64_t timestamp, uint32_t, UT_array *tags,
                      int) {
        auto *state = static_cast<Context *>(arg);
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->observed_complete = state->complete;
        }
        for (unsigned i = 0; i < utarray_len(tags); ++i) {
            auto *item = static_cast<neu_datatag_t *>(utarray_eltptr(tags, i));
            neu_type_e type       = NEU_TYPE_AUTO;
            uint64_t   generation = 0;
            if (timestamp == state->applied) {
                neu_driver_cache_type_get(state->cache, "g", item->name, &type,
                                          &generation);
            }
            (strcmp(item->name, "a") == 0 ? state->first : state->second) =
                type;
        }
    };
    neu_group_tag_query_t query = {};
    neu_group_query_snapshot(group, &query, &ctx, inspect);
    EXPECT_EQ(NEU_TYPE_AUTO, ctx.first);
    EXPECT_EQ(NEU_TYPE_AUTO, ctx.second);

    std::thread rebuild([&] {
        neu_group_change_test(
            group, 0, &ctx,
            [](void *arg, int64_t timestamp, UT_array *tags, uint32_t) {
                auto *state    = static_cast<Context *>(arg);
                state->applied = timestamp;
                {
                    std::unique_lock<std::mutex> lock(state->mutex);
                    state->rebuilding = true;
                    state->changed.notify_all();
                    state->changed.wait(lock, [&] { return state->release; });
                }
                for (unsigned i = 0; i < utarray_len(tags); ++i) {
                    auto *item =
                        static_cast<neu_datatag_t *>(utarray_eltptr(tags, i));
                    neu_dvalue_t value = {};
                    value.type         = NEU_TYPE_ERROR;
                    value.value.i32    = NEU_ERR_PLUGIN_TAG_NOT_READY;
                    neu_driver_cache_add_tag(state->cache, "g", item, value);
                }
                neu_dvalue_t value = {};
                value.type         = NEU_TYPE_DOUBLE;
                value.value.d64    = 1.5;
                neu_driver_cache_update(state->cache, "g", "a", 100, value,
                                        nullptr, 0);
                {
                    std::lock_guard<std::mutex> lock(state->mutex);
                    state->complete = true;
                }
                utarray_free(tags);
            });
    });
    {
        std::unique_lock<std::mutex> lock(ctx.mutex);
        ctx.changed.wait(lock, [&] { return ctx.rebuilding; });
    }
    std::thread reader([&] {
        {
            std::lock_guard<std::mutex> lock(ctx.mutex);
            ctx.query_started = true;
            ctx.changed.notify_all();
        }
        neu_group_query_snapshot(group, &query, &ctx, inspect);
    });
    {
        std::unique_lock<std::mutex> lock(ctx.mutex);
        ctx.changed.wait(lock, [&] { return ctx.query_started; });
        ctx.release = true;
        ctx.changed.notify_all();
    }
    rebuild.join();
    reader.join();
    EXPECT_TRUE(ctx.observed_complete);
    EXPECT_EQ(NEU_TYPE_DOUBLE, ctx.first);
    EXPECT_EQ(NEU_TYPE_AUTO, ctx.second);
    neu_group_destroy(group);
    neu_driver_cache_destroy(ctx.cache);
}

TEST(DriverAuto, ReadPathsScaleWithoutChangingConfig)
{
    for (int path : { 0, 1, 2 }) {
        neu_dvalue_t sample = {};
        sample.type         = NEU_TYPE_INT16;
        sample.value.i16    = -246;
        sample.attrs        = sample_attrs();
        neu_dvalue_t output = {};
        neu_type_e   config, resolved;
        auto_test_read(sample, 0.5, "sample", path, &output, &config,
                       &resolved);
        EXPECT_EQ(NEU_TYPE_INT16, resolved);
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
        neu_type_e   config, resolved;
        auto_test_read(sample, 0, "sample#B", path, &output, &config,
                       &resolved);
        EXPECT_EQ(NEU_TYPE_INT16, resolved);
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
                neu_type_e   config, resolved;
                auto_test_read(sample, 0, "sample", path, &output, &config,
                               &resolved);
                EXPECT_EQ(type == NEU_TYPE_PTR
                              ? NEU_TYPE_STRING
                              : type == NEU_TYPE_ERROR ? NEU_TYPE_AUTO : type,
                          resolved);
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
