#include <gtest/gtest.h>

#include <atomic>
#include <limits>
#include <thread>

extern "C" {
#include "adapter/driver/cache.h"
#include "errcodes.h"

bool sub_filter_err = false;
}

class DriverCacheSample : public testing::Test {
protected:
    void SetUp() override
    {
        cache                = neu_driver_cache_new();
        neu_dvalue_t initial = {};
        initial.type         = NEU_TYPE_ERROR;
        neu_driver_cache_add(cache, "group", "tag", initial);
    }

    void TearDown() override
    {
        neu_driver_cache_destroy(cache);
        sub_filter_err = false;
    }

    neu_dvalue_t sample()
    {
        neu_dvalue_t value               = {};
        value.type                       = NEU_TYPE_DOUBLE;
        value.value.d64                  = 1.25;
        value.attrs.has_timestamp        = true;
        value.attrs.timestamp            = 100;
        value.attrs.has_quality          = true;
        value.attrs.quality              = 192;
        value.attrs.has_source_timestamp = true;
        value.attrs.source_timestamp     = INT64_C(1712345678901);
        value.attrs.has_server_timestamp = true;
        value.attrs.server_timestamp     = -INT64_C(1712345678902);
        return value;
    }

    void expect_attrs(const neu_tag_sample_attrs_t &expected,
                      const neu_tag_sample_attrs_t &actual)
    {
        EXPECT_EQ(expected.has_timestamp, actual.has_timestamp);
        EXPECT_EQ(expected.timestamp, actual.timestamp);
        EXPECT_EQ(expected.has_quality, actual.has_quality);
        EXPECT_EQ(expected.quality, actual.quality);
        EXPECT_EQ(expected.has_source_timestamp, actual.has_source_timestamp);
        EXPECT_EQ(expected.source_timestamp, actual.source_timestamp);
        EXPECT_EQ(expected.has_server_timestamp, actual.has_server_timestamp);
        EXPECT_EQ(expected.server_timestamp, actual.server_timestamp);
    }

    bool update(neu_dvalue_t value)
    {
        return neu_driver_cache_update_change(cache, "group", "tag", 200, value,
                                              NULL, 0, false);
    }

    int changed()
    {
        return neu_driver_cache_meta_get_changed(cache, "group", "tag", &out,
                                                 &metas, &n_meta);
    }

    neu_driver_cache_t *     cache  = NULL;
    neu_driver_cache_value_t out    = {};
    neu_tag_meta_t *         metas  = NULL;
    int                      n_meta = 0;
};

TEST_F(DriverCacheSample, existing_update_preserves_optional_attrs)
{
    for (int flags = 0; flags < 16; ++flags) {
        SCOPED_TRACE(flags);
        neu_dvalue_t value               = sample();
        value.attrs.has_timestamp        = flags & 1;
        value.attrs.has_quality          = flags & 2;
        value.attrs.has_source_timestamp = flags & 4;
        value.attrs.has_server_timestamp = flags & 8;
        neu_driver_cache_update(cache, "group", "tag", 200, value, NULL, 0);

        ASSERT_EQ(0,
                  neu_driver_cache_meta_get(cache, "group", "tag", &out, &metas,
                                            &n_meta));
        EXPECT_EQ(200, out.timestamp);
        value.attrs.timestamp = flags & 1 ? value.attrs.timestamp : 0;
        value.attrs.quality   = flags & 2 ? value.attrs.quality : 0;
        value.attrs.source_timestamp =
            flags & 4 ? value.attrs.source_timestamp : 0;
        value.attrs.server_timestamp =
            flags & 8 ? value.attrs.server_timestamp : 0;
        expect_attrs(value.attrs, out.value.attrs);
    }
}

TEST_F(DriverCacheSample, timestamp_alone_does_not_trigger_change)
{
    neu_dvalue_t value = sample();
    ASSERT_TRUE(update(value));
    ASSERT_EQ(0, changed());

    struct {
        int64_t neu_tag_sample_attrs_t::*value;
        bool neu_tag_sample_attrs_t::*present;
    } timestamps[] = {
        { &neu_tag_sample_attrs_t::timestamp,
          &neu_tag_sample_attrs_t::has_timestamp },
        { &neu_tag_sample_attrs_t::source_timestamp,
          &neu_tag_sample_attrs_t::has_source_timestamp },
        { &neu_tag_sample_attrs_t::server_timestamp,
          &neu_tag_sample_attrs_t::has_server_timestamp },
    };
    for (const auto &timestamp : timestamps) {
        auto check = [&] {
            EXPECT_FALSE(update(value));
            EXPECT_NE(0, changed());
            ASSERT_EQ(0,
                      neu_driver_cache_meta_get(cache, "group", "tag", &out,
                                                &metas, &n_meta));
            neu_tag_sample_attrs_t expected = value.attrs;
            if (!(expected.*timestamp.present)) {
                expected.*timestamp.value = 0;
            }
            expect_attrs(expected, out.value.attrs);
        };
        value.attrs.*timestamp.value += 1;
        check();
        value.attrs.*timestamp.value = 0;
        check();
        value.attrs.*timestamp.value   = 123;
        value.attrs.*timestamp.present = false;
        check();
        value.attrs.*timestamp.value += 1;
        check();
        value.attrs.*timestamp.value   = 0;
        value.attrs.*timestamp.present = true;
        check();
    }
}

TEST_F(DriverCacheSample, explicit_zero_attrs_are_present)
{
    neu_dvalue_t value           = sample();
    value.attrs.timestamp        = 0;
    value.attrs.quality          = 0;
    value.attrs.source_timestamp = 0;
    value.attrs.server_timestamp = 0;
    ASSERT_TRUE(update(value));
    ASSERT_EQ(0, changed());
    expect_attrs(value.attrs, out.value.attrs);
    EXPECT_FALSE(update(value));
    EXPECT_NE(0, changed());
}

TEST_F(DriverCacheSample, quality_change_stays_pending_until_read)
{
    for (neu_type_e type : { NEU_TYPE_FLOAT, NEU_TYPE_DOUBLE }) {
        neu_dvalue_t value = sample();
        value.type         = type;
        if (type == NEU_TYPE_FLOAT) {
            value.value.f32 = 1.25f;
        }
        ASSERT_TRUE(update(value));
        ASSERT_EQ(0, changed());

        value.attrs.quality = 0;
        EXPECT_TRUE(update(value));
        value.attrs.timestamp++;
        value.attrs.source_timestamp++;
        value.attrs.has_server_timestamp = false;
        EXPECT_FALSE(update(value));
        ASSERT_EQ(0, changed());
        EXPECT_TRUE(out.value.attrs.has_quality);
        EXPECT_EQ(0, out.value.attrs.quality);
        EXPECT_EQ(value.attrs.source_timestamp,
                  out.value.attrs.source_timestamp);
        EXPECT_FALSE(out.value.attrs.has_server_timestamp);
        EXPECT_EQ(0, out.value.attrs.server_timestamp);
        EXPECT_NE(0, changed());
    }
}

TEST_F(DriverCacheSample, absent_attrs_clear_previous_sample)
{
    neu_dvalue_t value = sample();
    ASSERT_TRUE(update(value));
    ASSERT_EQ(0, changed());

    value.attrs = {};
    EXPECT_TRUE(update(value));
    ASSERT_EQ(0, changed());
    expect_attrs(value.attrs, out.value.attrs);
    EXPECT_FALSE(update(value));
    EXPECT_NE(0, changed());

    value.attrs.has_quality = true;
    EXPECT_TRUE(update(value));
    ASSERT_EQ(0, changed());
    expect_attrs(value.attrs, out.value.attrs);
}

TEST_F(DriverCacheSample, forced_update_preserves_attrs)
{
    neu_dvalue_t value = sample();
    ASSERT_TRUE(update(value));
    ASSERT_EQ(0, changed());

    value.attrs.timestamp++;
    value.attrs.source_timestamp     = 0;
    value.attrs.has_server_timestamp = false;
    EXPECT_FALSE(neu_driver_cache_update_change(cache, "group", "tag", 300,
                                                value, NULL, 0, true));
    ASSERT_EQ(0, changed());
    EXPECT_EQ(300, out.timestamp);
    value.attrs.server_timestamp = 0;
    expect_attrs(value.attrs, out.value.attrs);
    EXPECT_NE(0, changed());
}

TEST_F(DriverCacheSample, errors_and_recovery_replace_attrs)
{
    neu_dvalue_t value = sample();
    ASSERT_TRUE(update(value));
    ASSERT_EQ(0, changed());

    neu_dvalue_t error           = sample();
    error.type                   = NEU_TYPE_ERROR;
    error.value.i32              = 1;
    error.attrs.source_timestamp = std::numeric_limits<int64_t>::min();
    error.attrs.server_timestamp = std::numeric_limits<int64_t>::max();
    EXPECT_TRUE(update(error));
    ASSERT_EQ(0, changed());
    EXPECT_EQ(NEU_TYPE_ERROR, out.value.type);
    EXPECT_EQ(1, out.value.value.i32);
    expect_attrs(error.attrs, out.value.attrs);

    value.attrs = {};
    EXPECT_TRUE(update(value));
    ASSERT_EQ(0, changed());
    EXPECT_EQ(NEU_TYPE_DOUBLE, out.value.type);
    expect_attrs(value.attrs, out.value.attrs);
    EXPECT_FALSE(update(value));
    EXPECT_NE(0, changed());

    error.attrs.has_timestamp        = false;
    error.attrs.has_quality          = false;
    error.attrs.has_source_timestamp = false;
    error.attrs.has_server_timestamp = false;
    EXPECT_TRUE(update(error));
    ASSERT_EQ(0, changed());
    expect_attrs(value.attrs, out.value.attrs);
}

TEST_F(DriverCacheSample, filtered_error_compares_last_good_quality)
{
    sub_filter_err     = true;
    neu_dvalue_t value = sample();
    ASSERT_TRUE(update(value));
    ASSERT_EQ(0, changed());

    neu_dvalue_t error = sample();
    error.type         = NEU_TYPE_ERROR;
    error.value.i32    = 1;
    error.attrs        = {};
    EXPECT_FALSE(update(error));
    EXPECT_NE(0, changed());
    ASSERT_EQ(0,
              neu_driver_cache_meta_get(cache, "group", "tag", &out, &metas,
                                        &n_meta));
    EXPECT_EQ(NEU_TYPE_ERROR, out.value.type);
    expect_attrs(error.attrs, out.value.attrs);

    value.attrs.timestamp++;
    value.attrs.source_timestamp     = 0;
    value.attrs.has_server_timestamp = false;
    EXPECT_FALSE(update(value));
    EXPECT_NE(0, changed());
    ASSERT_EQ(0,
              neu_driver_cache_meta_get(cache, "group", "tag", &out, &metas,
                                        &n_meta));
    value.attrs.server_timestamp = 0;
    expect_attrs(value.attrs, out.value.attrs);

    EXPECT_FALSE(update(error));
    value.attrs.quality = 0;
    EXPECT_TRUE(update(value));
    ASSERT_EQ(0, changed());
    expect_attrs(value.attrs, out.value.attrs);

    EXPECT_FALSE(update(error));
    value.attrs.has_source_timestamp = false;
    EXPECT_FALSE(update(value));
    EXPECT_NE(0, changed());

    EXPECT_FALSE(update(error));
    value.attrs.has_quality = false;
    EXPECT_TRUE(update(value));
    ASSERT_EQ(0, changed());
    expect_attrs(value.attrs, out.value.attrs);

    EXPECT_FALSE(update(error));
    value.attrs.has_quality = true;
    EXPECT_TRUE(update(value));
    ASSERT_EQ(0, changed());
    expect_attrs(value.attrs, out.value.attrs);
}

TEST_F(DriverCacheSample, copy_dvalue_preserves_attrs)
{
    for (int flags = 0; flags < 16; ++flags) {
        SCOPED_TRACE(flags);
        neu_dvalue_t src               = sample();
        src.attrs.has_timestamp        = flags & 1;
        src.attrs.has_quality          = flags & 2;
        src.attrs.has_source_timestamp = flags & 4;
        src.attrs.has_server_timestamp = flags & 8;
        neu_dvalue_t dst               = {};
        neu_copy_dvalue(&src, &dst);
        EXPECT_EQ(src.type, dst.type);
        EXPECT_EQ(src.value.d64, dst.value.d64);
        expect_attrs(src.attrs, dst.attrs);
    }
}

class DriverCacheAuto : public DriverCacheSample {
protected:
    void SetUp() override
    {
        DriverCacheSample::SetUp();
        configure();
    }

    void TearDown() override
    {
        release();
        DriverCacheSample::TearDown();
    }

    void configure(neu_type_e type = NEU_TYPE_AUTO, uint8_t precision = 0,
                   double decimal = 0, double bias = 0)
    {
        neu_datatag_t tag    = {};
        tag.name             = const_cast<char *>("tag");
        tag.type             = type;
        tag.precision        = precision;
        tag.decimal          = decimal;
        tag.bias             = bias;
        neu_dvalue_t initial = {};
        initial.type         = NEU_TYPE_ERROR;
        initial.value.i32    = NEU_ERR_PLUGIN_TAG_NOT_READY;
        neu_driver_cache_add_tag(cache, "group", &tag, initial);
    }

    int checked(neu_dvalue_t value, bool force = false)
    {
        return neu_driver_cache_update_checked(
            cache, "group", "tag", 200, value, NULL, 0, force, &was_changed);
    }

    void release()
    {
        if (out.value.type == NEU_TYPE_PTR) {
            free(out.value.value.ptr.ptr);
        } else {
            neu_free_dvalue(&out.value);
        }
        out = {};
        for (int i = 0; i < n_meta; i++) {
            if (metas[i].value.type == NEU_TYPE_PTR) {
                free(metas[i].value.value.ptr.ptr);
            } else {
                neu_free_dvalue(&metas[i].value);
            }
        }
        free(metas);
        metas  = NULL;
        n_meta = 0;
    }

    int read(bool only_changed = false)
    {
        release();
        return only_changed ? neu_driver_cache_meta_get_changed(
                                  cache, "group", "tag", &out, &metas, &n_meta)
                            : neu_driver_cache_meta_get(cache, "group", "tag",
                                                        &out, &metas, &n_meta);
    }

    int type_get()
    {
        return neu_driver_cache_type_get(cache, "group", "tag", &type,
                                         &generation);
    }

    neu_dvalue_t error()
    {
        neu_dvalue_t value = {};
        value.type         = NEU_TYPE_ERROR;
        value.value.i32    = 123;
        return value;
    }

    neu_dvalue_t dynamic(neu_type_e type, const char *text = "value")
    {
        neu_dvalue_t value = {};
        value.type         = type;
        if (type == NEU_TYPE_PTR) {
            value.value.ptr.type   = NEU_TYPE_STRING;
            value.value.ptr.length = strlen(text) + 1;
            value.value.ptr.ptr =
                reinterpret_cast<uint8_t *>(const_cast<char *>(text));
        } else if (type == NEU_TYPE_ARRAY_STRING) {
            value.value.strs.length = 1;
            value.value.strs.strs =
                static_cast<char **>(calloc(1, sizeof(char *)));
            value.value.strs.strs[0] = strdup(text);
        } else if (type == NEU_TYPE_CUSTOM) {
            value.value.json = json_pack("{s:s}", "key", text);
        } else {
            value.value.i32s.length = 1;
            value.value.i32s.i32s =
                static_cast<int32_t *>(calloc(1, sizeof(int32_t)));
            value.value.i32s.i32s[0] = text[0];
        }
        return value;
    }

    bool       was_changed = false;
    neu_type_e type        = NEU_TYPE_AUTO;
    uint64_t   generation  = 0;
};

TEST_F(DriverCacheAuto, copy_dynamic_dvalue_preserves_attrs_and_ownership)
{
    for (neu_type_e kind : { NEU_TYPE_PTR, NEU_TYPE_ARRAY_INT32,
                             NEU_TYPE_ARRAY_STRING, NEU_TYPE_CUSTOM }) {
        SCOPED_TRACE(kind);
        neu_dvalue_t src = dynamic(kind);
        src.attrs        = sample().attrs;
        neu_dvalue_t dst = {};
        neu_copy_dvalue(&src, &dst);
        EXPECT_EQ(src.type, dst.type);
        expect_attrs(src.attrs, dst.attrs);
        if (kind == NEU_TYPE_PTR) {
            EXPECT_EQ(src.value.ptr.ptr, dst.value.ptr.ptr);
            EXPECT_EQ(src.value.ptr.length, dst.value.ptr.length);
            EXPECT_EQ(src.value.ptr.type, dst.value.ptr.type);
        } else if (kind == NEU_TYPE_ARRAY_INT32) {
            EXPECT_NE(src.value.i32s.i32s, dst.value.i32s.i32s);
            EXPECT_EQ(1u, dst.value.i32s.length);
            EXPECT_EQ('v', dst.value.i32s.i32s[0]);
            dst.value.i32s.i32s[0] = 'x';
            EXPECT_EQ('v', src.value.i32s.i32s[0]);
        } else if (kind == NEU_TYPE_ARRAY_STRING) {
            EXPECT_NE(src.value.strs.strs, dst.value.strs.strs);
            EXPECT_NE(src.value.strs.strs[0], dst.value.strs.strs[0]);
            EXPECT_EQ(1u, dst.value.strs.length);
            EXPECT_STREQ("value", dst.value.strs.strs[0]);
            dst.value.strs.strs[0][0] = 'x';
            EXPECT_STREQ("value", src.value.strs.strs[0]);
        } else {
            EXPECT_NE(src.value.json, dst.value.json);
            EXPECT_TRUE(json_equal(src.value.json, dst.value.json));
            json_object_set_new(dst.value.json, "key", json_string("other"));
            EXPECT_STREQ(
                "value",
                json_string_value(json_object_get(src.value.json, "key")));
        }
        neu_free_dvalue(&src);
        neu_free_dvalue(&dst);
    }
}

TEST_F(DriverCacheAuto, dynamic_cache_attrs_and_filtered_last_good_are_owned)
{
    sub_filter_err = true;
    for (neu_type_e kind : { NEU_TYPE_PTR, NEU_TYPE_ARRAY_INT32,
                             NEU_TYPE_ARRAY_STRING, NEU_TYPE_CUSTOM }) {
        SCOPED_TRACE(kind);
        configure();
        char         text[]             = "value";
        neu_dvalue_t value              = dynamic(kind, text);
        value.attrs                     = sample().attrs;
        neu_tag_sample_attrs_t expected = value.attrs;
        ASSERT_EQ(NEU_ERR_SUCCESS, checked(value));
        text[0] = 'x';
        ASSERT_EQ(0, read(true));
        expect_attrs(expected, out.value.attrs);
        if (kind == NEU_TYPE_PTR) {
            EXPECT_NE(value.value.ptr.ptr, out.value.value.ptr.ptr);
            EXPECT_EQ('v', out.value.value.ptr.ptr[0]);
            out.value.value.ptr.ptr[0] = 'z';
        }
        out.value.attrs.quality          = 0;
        out.value.attrs.source_timestamp = 42;
        out.value.attrs.server_timestamp = 43;
        ASSERT_EQ(0, read());
        expect_attrs(expected, out.value.attrs);
        if (kind == NEU_TYPE_PTR) {
            EXPECT_EQ('v', out.value.value.ptr.ptr[0]);
        }

        ASSERT_EQ(NEU_ERR_SUCCESS, checked(error()));
        EXPECT_FALSE(was_changed);
        EXPECT_NE(0, read(true));
        ASSERT_EQ(0, read());
        expect_attrs(error().attrs, out.value.attrs);

        value       = dynamic(kind);
        value.attrs = expected;
        value.attrs.timestamp++;
        value.attrs.has_source_timestamp = false;
        value.attrs.server_timestamp     = 0;
        ASSERT_EQ(NEU_ERR_SUCCESS, checked(value));
        EXPECT_FALSE(was_changed);
        EXPECT_NE(0, read(true));
        ASSERT_EQ(0, read());
        expected                  = value.attrs;
        expected.source_timestamp = 0;
        expect_attrs(expected, out.value.attrs);

        ASSERT_EQ(NEU_ERR_SUCCESS, checked(error()));
        value               = dynamic(kind);
        value.attrs         = expected;
        value.attrs.quality = 0;
        ASSERT_EQ(NEU_ERR_SUCCESS, checked(value));
        EXPECT_TRUE(was_changed);
        ASSERT_EQ(0, read(true));
        expected.quality = 0;
        expect_attrs(expected, out.value.attrs);
    }
}

TEST_F(DriverCacheAuto, unresolved_error_then_first_success)
{
    EXPECT_EQ(0, NEU_TYPE_AUTO);
    EXPECT_STREQ("NEU_TYPE_AUTO", neu_type_string(NEU_TYPE_AUTO));
    ASSERT_EQ(NEU_ERR_PLUGIN_TAG_NOT_READY, type_get());
    EXPECT_EQ(NEU_TYPE_AUTO, type);
    uint64_t registered = generation;
    EXPECT_EQ(NEU_ERR_SUCCESS, checked(error()));
    EXPECT_EQ(NEU_ERR_PLUGIN_TAG_NOT_READY, type_get());
    EXPECT_EQ(registered, generation);
    ASSERT_EQ(NEU_ERR_SUCCESS, checked(sample()));
    EXPECT_TRUE(was_changed);
    ASSERT_EQ(NEU_ERR_SUCCESS, type_get());
    EXPECT_EQ(NEU_TYPE_DOUBLE, type);
    ASSERT_EQ(0, read(true));
    EXPECT_EQ(type, out.resolved_type);
    EXPECT_EQ(registered, out.generation);
    EXPECT_EQ(NEU_TYPE_DOUBLE, out.value.type);
    EXPECT_EQ(NEU_ERR_SUCCESS, checked(sample()));
    EXPECT_FALSE(was_changed);
    EXPECT_NE(0, read(true));
}

TEST_F(DriverCacheAuto, mismatch_publishes_error_and_original_type_recovers)
{
    ASSERT_EQ(NEU_ERR_SUCCESS, checked(sample()));
    ASSERT_EQ(0, read(true));
    uint64_t     registered = out.generation;
    neu_dvalue_t wrong      = {};
    wrong.type              = NEU_TYPE_INT16;
    wrong.value.i16         = 42;
    EXPECT_EQ(NEU_ERR_PLUGIN_TAG_TYPE_MISMATCH, checked(wrong));
    EXPECT_TRUE(was_changed);
    ASSERT_EQ(0, read(true));
    EXPECT_EQ(NEU_TYPE_ERROR, out.value.type);
    EXPECT_EQ(NEU_ERR_PLUGIN_TAG_TYPE_MISMATCH, out.value.value.i32);
    EXPECT_EQ(NEU_TYPE_DOUBLE, out.resolved_type);
    EXPECT_EQ(registered, out.generation);
    EXPECT_EQ(NEU_ERR_SUCCESS, type_get());
    EXPECT_EQ(NEU_TYPE_DOUBLE, type);
    ASSERT_EQ(NEU_ERR_SUCCESS, checked(error()));
    ASSERT_EQ(0, read());
    EXPECT_EQ(NEU_TYPE_DOUBLE, out.resolved_type);
    EXPECT_EQ(NEU_ERR_SUCCESS, checked(sample()));
    EXPECT_TRUE(was_changed);
    ASSERT_EQ(0, read(true));
    EXPECT_EQ(1.25, out.value.value.d64);
    EXPECT_EQ(registered, out.generation);
}

TEST_F(DriverCacheAuto, generation_changes_on_rebuild_not_resolution_or_rename)
{
    ASSERT_EQ(NEU_ERR_PLUGIN_TAG_NOT_READY, type_get());
    uint64_t first = generation;
    ASSERT_EQ(NEU_ERR_SUCCESS, checked(sample()));
    ASSERT_EQ(NEU_ERR_SUCCESS, type_get());
    EXPECT_EQ(first, generation);
    neu_driver_cache_rename(cache, "group", "tag", "renamed");
    EXPECT_EQ(NEU_ERR_SUCCESS,
              neu_driver_cache_type_get(cache, "group", "renamed", &type,
                                        &generation));
    EXPECT_EQ(first, generation);
    neu_driver_cache_del(cache, "group", "renamed");
    EXPECT_EQ(NEU_ERR_PLUGIN_TAG_NOT_READY,
              neu_driver_cache_type_get(cache, "group", "renamed", &type,
                                        &generation));
    configure();
    ASSERT_EQ(NEU_ERR_PLUGIN_TAG_NOT_READY, type_get());
    EXPECT_GT(generation, first);
    uint64_t second = generation;
    configure();
    ASSERT_EQ(NEU_ERR_PLUGIN_TAG_NOT_READY, type_get());
    EXPECT_GT(generation, second);
    neu_dvalue_t value  = {};
    value.type          = NEU_TYPE_BOOL;
    value.value.boolean = true;
    ASSERT_EQ(NEU_ERR_SUCCESS, checked(value));
    ASSERT_EQ(NEU_ERR_SUCCESS, type_get());
    EXPECT_EQ(NEU_TYPE_BOOL, type);
}

TEST_F(DriverCacheAuto, explicit_type_resolves_before_success)
{
    configure(NEU_TYPE_INT16);
    ASSERT_EQ(NEU_ERR_SUCCESS, type_get());
    EXPECT_EQ(NEU_TYPE_INT16, type);
    ASSERT_EQ(0, read());
    EXPECT_EQ(NEU_TYPE_ERROR, out.value.type);
    EXPECT_EQ(NEU_TYPE_INT16, out.resolved_type);
    EXPECT_EQ(generation, out.generation);
}

TEST_F(DriverCacheAuto, rejects_nonsemantic_and_unsupported_samples)
{
    for (neu_type_e invalid :
         { NEU_TYPE_AUTO, static_cast<neu_type_e>(35), NEU_TYPE_PTR }) {
        neu_dvalue_t value = {};
        value.type         = invalid;
        if (invalid == NEU_TYPE_PTR) {
            value.value.ptr.type = NEU_TYPE_ERROR;
        }
        EXPECT_EQ(NEU_ERR_PLUGIN_TAG_TYPE_MISMATCH, checked(value));
        EXPECT_EQ(NEU_ERR_PLUGIN_TAG_NOT_READY, type_get());
        ASSERT_EQ(0, read());
        EXPECT_EQ(NEU_TYPE_ERROR, out.value.type);
        EXPECT_EQ(NEU_TYPE_AUTO, out.resolved_type);
    }
    for (neu_type_e invalid : { NEU_TYPE_AUTO, NEU_TYPE_ERROR, NEU_TYPE_PTR,
                                static_cast<neu_type_e>(35) }) {
        configure(invalid);
        EXPECT_EQ(NEU_ERR_PLUGIN_TAG_NOT_READY, type_get());
    }
}

TEST_F(DriverCacheAuto, first_sample_validates_deferred_numeric_configuration)
{
    neu_dvalue_t string = {};
    string.type         = NEU_TYPE_STRING;
    strcpy(string.value.str, "hello");
    for (int option = 0; option < 3; option++) {
        configure(NEU_TYPE_AUTO, option == 2 ? 2 : 0, option == 0 ? 0.1 : 0,
                  option == 1 ? 1 : 0);
        int expected = option == 0
            ? NEU_ERR_TAG_DECIMAL_INVALID
            : option == 1 ? NEU_ERR_TAG_BIAS_INVALID
                          : NEU_ERR_TAG_PRECISION_INVALID;
        EXPECT_EQ(expected, checked(string));
        EXPECT_EQ(NEU_ERR_PLUGIN_TAG_NOT_READY, type_get());
        ASSERT_EQ(0, read());
        EXPECT_EQ(expected, out.value.value.i32);
        ASSERT_EQ(NEU_ERR_SUCCESS, checked(sample()));
        ASSERT_EQ(NEU_ERR_SUCCESS, type_get());
        EXPECT_EQ(NEU_TYPE_DOUBLE, type);
    }
}

TEST_F(DriverCacheAuto, configured_precision_used_for_current_and_last_good)
{
    for (neu_type_e numeric : { NEU_TYPE_FLOAT, NEU_TYPE_DOUBLE }) {
        for (bool filtered : { false, true }) {
            configure(NEU_TYPE_AUTO, 2);
            sub_filter_err     = filtered;
            neu_dvalue_t value = {};
            value.type         = numeric;
            if (numeric == NEU_TYPE_FLOAT) {
                value.value.f32 = 1.0f;
            } else {
                value.value.d64 = 1.0;
            }
            ASSERT_EQ(NEU_ERR_SUCCESS, checked(value));
            ASSERT_EQ(0, read(true));
            EXPECT_EQ(2, out.value.precision);
            if (filtered) {
                EXPECT_EQ(NEU_ERR_SUCCESS, checked(error()));
                EXPECT_FALSE(was_changed);
            }
            if (numeric == NEU_TYPE_FLOAT) {
                value.value.f32 = 1.005f;
            } else {
                value.value.d64 = 1.005;
            }
            value.precision = 7;
            EXPECT_EQ(NEU_ERR_SUCCESS, checked(value));
            EXPECT_FALSE(was_changed);
            EXPECT_NE(0, read(true));
            if (filtered) {
                EXPECT_EQ(NEU_ERR_SUCCESS, checked(error()));
            }
            if (numeric == NEU_TYPE_FLOAT) {
                value.value.f32 = 1.1f;
            } else {
                value.value.d64 = 1.1;
            }
            EXPECT_EQ(NEU_ERR_SUCCESS, checked(value));
            EXPECT_TRUE(was_changed);
            ASSERT_EQ(0, read(true));
            EXPECT_EQ(2, out.value.precision);
        }
    }
}

TEST_F(DriverCacheAuto, attrs_and_force_use_existing_change_semantics)
{
    neu_dvalue_t value = sample();
    ASSERT_EQ(NEU_ERR_SUCCESS, checked(value));
    ASSERT_EQ(0, read(true));
    value.attrs.timestamp++;
    EXPECT_EQ(NEU_ERR_SUCCESS, checked(value));
    EXPECT_FALSE(was_changed);
    EXPECT_NE(0, read(true));
    value.attrs.quality++;
    EXPECT_EQ(NEU_ERR_SUCCESS, checked(value));
    EXPECT_TRUE(was_changed);
    ASSERT_EQ(0, read(true));
    EXPECT_EQ(value.attrs.quality, out.value.attrs.quality);
    value.attrs.has_timestamp = false;
    EXPECT_EQ(NEU_ERR_SUCCESS, checked(value, true));
    EXPECT_FALSE(was_changed);
    ASSERT_EQ(0, read(true));
    EXPECT_FALSE(out.value.attrs.has_timestamp);
    EXPECT_EQ(0, out.value.attrs.timestamp);
    EXPECT_EQ(NEU_TYPE_DOUBLE, out.resolved_type);
}

TEST_F(DriverCacheAuto, ptr_is_copied_and_resolves_string_or_bytes)
{
    for (neu_type_e semantic : { NEU_TYPE_STRING, NEU_TYPE_BYTES }) {
        configure();
        char         text[]  = "hello";
        neu_dvalue_t value   = dynamic(NEU_TYPE_PTR, text);
        value.value.ptr.type = semantic;
        ASSERT_EQ(NEU_ERR_SUCCESS, checked(value));
        text[0] = 'x';
        ASSERT_EQ(NEU_ERR_SUCCESS, type_get());
        EXPECT_EQ(semantic, type);
        ASSERT_EQ(0, read(true));
        EXPECT_EQ(NEU_TYPE_PTR, out.value.type);
        EXPECT_EQ(semantic, out.value.value.ptr.type);
        EXPECT_EQ('h', out.value.value.ptr.ptr[0]);
        EXPECT_EQ(semantic, out.resolved_type);
        value.value.ptr.type =
            semantic == NEU_TYPE_STRING ? NEU_TYPE_BYTES : NEU_TYPE_STRING;
        EXPECT_EQ(NEU_ERR_PLUGIN_TAG_TYPE_MISMATCH, checked(value));
        ASSERT_EQ(NEU_ERR_SUCCESS, type_get());
        EXPECT_EQ(semantic, type);
    }
}

TEST_F(DriverCacheAuto, dynamic_error_recovery_and_filtered_last_good_lifetimes)
{
    for (neu_type_e dynamic_type : { NEU_TYPE_PTR, NEU_TYPE_ARRAY_INT32,
                                     NEU_TYPE_ARRAY_STRING, NEU_TYPE_CUSTOM }) {
        for (bool filtered : { false, true }) {
            configure();
            sub_filter_err = filtered;
            ASSERT_EQ(NEU_ERR_SUCCESS, checked(dynamic(dynamic_type)));
            ASSERT_EQ(0, read(true));
            EXPECT_EQ(NEU_ERR_SUCCESS, checked(error()));
            EXPECT_EQ(!filtered, was_changed);
            if (filtered) {
                EXPECT_NE(0, read(true));
            } else {
                ASSERT_EQ(0, read(true));
                EXPECT_EQ(NEU_TYPE_ERROR, out.value.type);
            }
            EXPECT_EQ(NEU_ERR_SUCCESS, checked(dynamic(dynamic_type)));
            EXPECT_EQ(!filtered, was_changed);
            if (filtered) {
                EXPECT_NE(0, read(true));
            } else {
                ASSERT_EQ(0, read(true));
            }
            EXPECT_EQ(NEU_ERR_SUCCESS, checked(error()));
            EXPECT_EQ(NEU_ERR_SUCCESS, checked(dynamic(dynamic_type, "other")));
            EXPECT_TRUE(was_changed);
            ASSERT_EQ(0, read(true));
            EXPECT_EQ(dynamic_type, out.value.type);
            configure();
        }
    }
}

TEST_F(DriverCacheAuto, rejected_dynamic_samples_are_disposed)
{
    sub_filter_err = true;
    ASSERT_EQ(NEU_ERR_SUCCESS, checked(sample()));
    ASSERT_EQ(0, read(true));
    for (neu_type_e dynamic_type :
         { NEU_TYPE_ARRAY_INT32, NEU_TYPE_ARRAY_STRING, NEU_TYPE_CUSTOM }) {
        neu_dvalue_t value = dynamic(dynamic_type);
        json_t *     json  = dynamic_type == NEU_TYPE_CUSTOM
            ? json_incref(value.value.json)
            : NULL;
        EXPECT_EQ(NEU_ERR_PLUGIN_TAG_TYPE_MISMATCH, checked(value));
        EXPECT_FALSE(was_changed);
        EXPECT_NE(0, read(true));
        if (json) {
            EXPECT_EQ(1u, json->refcount);
            json_decref(json);
        }
        EXPECT_EQ(NEU_ERR_SUCCESS, checked(sample()));
        EXPECT_FALSE(was_changed);
    }
    neu_driver_cache_del(cache, "group", "tag");
    EXPECT_EQ(NEU_ERR_PLUGIN_TAG_NOT_READY,
              checked(dynamic(NEU_TYPE_ARRAY_STRING)));
}

TEST_F(DriverCacheAuto, filtered_first_zero_success_is_a_change)
{
    sub_filter_err    = true;
    neu_dvalue_t zero = {};
    zero.type         = NEU_TYPE_INT32;
    EXPECT_EQ(NEU_ERR_SUCCESS, checked(zero));
    EXPECT_TRUE(was_changed);
    ASSERT_EQ(0, read(true));
    EXPECT_EQ(NEU_TYPE_INT32, out.resolved_type);
}

TEST_F(DriverCacheAuto, snapshots_pair_value_with_registration_and_type)
{
    std::atomic<bool> done(false);
    std::thread       writer([&] {
        for (int i = 0; i < 2000; i++) {
            configure();
            neu_dvalue_t value = {};
            value.type         = i % 2 ? NEU_TYPE_DOUBLE : NEU_TYPE_INT32;
            neu_driver_cache_update(cache, "group", "tag", i, value, NULL, 0);
        }
        done = true;
    });
    uint64_t          previous = 0;
    do {
        int ret = read();
        EXPECT_EQ(0, ret);
        if (ret == 0) {
            EXPECT_GE(out.generation, previous);
            previous = out.generation;
            if (out.value.type != NEU_TYPE_ERROR) {
                EXPECT_EQ(out.value.type, out.resolved_type);
            }
        }
    } while (!done);
    writer.join();
}

TEST_F(DriverCacheAuto, nonfinite_samples_do_not_resolve_or_clear_type)
{
    for (neu_type_e numeric : { NEU_TYPE_FLOAT, NEU_TYPE_DOUBLE }) {
        configure();
        for (double invalid : { std::numeric_limits<double>::quiet_NaN(),
                                std::numeric_limits<double>::infinity(),
                                -std::numeric_limits<double>::infinity() }) {
            neu_dvalue_t value = {};
            value.type         = numeric;
            if (numeric == NEU_TYPE_FLOAT) {
                value.value.f32 = invalid;
            } else {
                value.value.d64 = invalid;
            }
            EXPECT_EQ(NEU_ERR_PLUGIN_TAG_VALUE_OUT_OF_RANGE, checked(value));
            EXPECT_EQ(NEU_ERR_PLUGIN_TAG_NOT_READY, type_get());
            ASSERT_EQ(0, read());
            EXPECT_EQ(NEU_TYPE_AUTO, out.resolved_type);
        }
        neu_dvalue_t valid = {};
        valid.type         = numeric;
        EXPECT_EQ(NEU_ERR_SUCCESS, checked(valid));
        EXPECT_EQ(NEU_ERR_SUCCESS, type_get());
        EXPECT_EQ(numeric, type);
    }
}

TEST_F(DriverCacheAuto, initial_success_is_validated_and_owned)
{
    neu_datatag_t tag = {};
    tag.name          = const_cast<char *>("tag");
    tag.type          = NEU_TYPE_AUTO;
    tag.precision     = 2;
    neu_driver_cache_add_tag(cache, "group", &tag, dynamic(NEU_TYPE_CUSTOM));
    EXPECT_EQ(NEU_ERR_PLUGIN_TAG_NOT_READY, type_get());
    ASSERT_EQ(0, read());
    EXPECT_EQ(NEU_ERR_TAG_PRECISION_INVALID, out.value.value.i32);
    sub_filter_err = true;
    neu_driver_cache_add_tag(cache, "group", &tag, sample());
    ASSERT_EQ(NEU_ERR_SUCCESS, type_get());
    EXPECT_EQ(NEU_TYPE_DOUBLE, type);
    ASSERT_EQ(0, read(true));
    EXPECT_EQ(2, out.value.precision);
    EXPECT_EQ(NEU_ERR_SUCCESS, checked(error()));
    EXPECT_EQ(NEU_ERR_SUCCESS, checked(sample()));
    EXPECT_FALSE(was_changed);
    EXPECT_NE(0, read(true));
}

TEST_F(DriverCacheAuto, every_numeric_array_owns_current_last_good_and_snapshot)
{
    sub_filter_err = true;
    for (int kind = NEU_TYPE_ARRAY_INT8; kind <= NEU_TYPE_ARRAY_BOOL; kind++) {
        configure();
        neu_dvalue_t empty = {};
        empty.type         = static_cast<neu_type_e>(kind);
        ASSERT_EQ(NEU_ERR_SUCCESS, checked(empty));
        ASSERT_EQ(0, read(true));
        EXPECT_EQ(empty.type, out.resolved_type);
        EXPECT_EQ(0u, out.value.value.bools.length);
        EXPECT_EQ(NEU_ERR_SUCCESS, checked(empty));
        EXPECT_FALSE(was_changed);
        auto array = [&] {
            neu_dvalue_t value       = {};
            value.type               = empty.type;
            value.value.bools.length = 2;
            value.value.bools.bools =
                static_cast<bool *>(calloc(2, sizeof(double)));
            return value;
        };
        ASSERT_EQ(NEU_ERR_SUCCESS, checked(array()));
        EXPECT_TRUE(was_changed);
        ASSERT_EQ(0, read(true));
        EXPECT_EQ(2u, out.value.value.bools.length);
        out.value.value.bools.bools[0] = true;
        ASSERT_EQ(0, read());
        EXPECT_FALSE(out.value.value.bools.bools[0]);
        EXPECT_EQ(NEU_ERR_SUCCESS, checked(error()));
        EXPECT_EQ(NEU_ERR_SUCCESS, checked(array()));
        EXPECT_FALSE(was_changed);
        EXPECT_NE(0, read(true));
    }
}

TEST_F(DriverCacheAuto, metadata_snapshots_copy_ptr_and_preserve_attrs)
{
    neu_tag_meta_t input[2] = {};
    strcpy(input[0].name, "text");
    char text[]                      = "hello";
    input[0].value                   = dynamic(NEU_TYPE_PTR, text);
    input[0].value.attrs.has_quality = true;
    input[0].value.attrs.quality     = 192;
    input[1].value                   = dynamic(NEU_TYPE_ARRAY_STRING);
    ASSERT_EQ(NEU_ERR_SUCCESS,
              neu_driver_cache_update_checked(cache, "group", "tag", 200,
                                              sample(), input, 2, false,
                                              &was_changed));
    text[0] = 'x';
    ASSERT_EQ(0, read(true));
    ASSERT_EQ(2, n_meta);
    EXPECT_STREQ("text", metas[0].name);
    EXPECT_EQ('h', metas[0].value.value.ptr.ptr[0]);
    EXPECT_EQ(192, metas[0].value.attrs.quality);
    EXPECT_EQ(NEU_TYPE_INT8, metas[1].value.type);
    metas[0].value.value.ptr.ptr[0] = 'z';
    ASSERT_EQ(0, read());
    EXPECT_EQ('h', metas[0].value.value.ptr.ptr[0]);
    input[0].value = dynamic(NEU_TYPE_PTR, "replacement");
    ASSERT_EQ(NEU_ERR_SUCCESS,
              neu_driver_cache_update_checked(cache, "group", "tag", 201,
                                              sample(), input, 1, false,
                                              &was_changed));
    ASSERT_EQ(0, read());
    EXPECT_EQ(1, n_meta);
    EXPECT_EQ('r', metas[0].value.value.ptr.ptr[0]);
}

TEST_F(DriverCacheAuto, generation_is_local_to_cache_instance)
{
    neu_driver_cache_t *other = neu_driver_cache_new();
    neu_datatag_t       tag   = {};
    tag.name                  = const_cast<char *>("tag");
    tag.type                  = NEU_TYPE_BOOL;
    neu_driver_cache_add_tag(other, "group", &tag, error());
    ASSERT_EQ(
        NEU_ERR_SUCCESS,
        neu_driver_cache_type_get(other, "group", "tag", &type, &generation));
    EXPECT_EQ(1u, generation);
    neu_driver_cache_destroy(other);
}

TEST_F(DriverCacheAuto, scalar_comparisons_ignore_unused_union_bytes)
{
    for (neu_type_e scalar :
         { NEU_TYPE_INT8, NEU_TYPE_UINT8, NEU_TYPE_INT16, NEU_TYPE_UINT16,
           NEU_TYPE_INT32, NEU_TYPE_UINT32, NEU_TYPE_INT64, NEU_TYPE_UINT64,
           NEU_TYPE_WORD, NEU_TYPE_DWORD, NEU_TYPE_LWORD, NEU_TYPE_BIT,
           NEU_TYPE_BOOL, NEU_TYPE_STRING, NEU_TYPE_TIME,
           NEU_TYPE_DATA_AND_TIME }) {
        for (bool filtered : { false, true }) {
            configure();
            sub_filter_err     = filtered;
            neu_dvalue_t value = {};
            value.type         = scalar;
            memset(&value.value, 0xa5, sizeof(value.value));
            if (scalar == NEU_TYPE_STRING || scalar == NEU_TYPE_TIME ||
                scalar == NEU_TYPE_DATA_AND_TIME) {
                strcpy(value.value.str, "same");
            } else {
                value.value.u64 = 0;
            }
            ASSERT_EQ(NEU_ERR_SUCCESS, checked(value));
            ASSERT_EQ(0, read(true));
            if (filtered) {
                ASSERT_EQ(NEU_ERR_SUCCESS, checked(error()));
            }
            memset(&value.value, 0, sizeof(value.value));
            if (scalar == NEU_TYPE_STRING || scalar == NEU_TYPE_TIME ||
                scalar == NEU_TYPE_DATA_AND_TIME) {
                strcpy(value.value.str, "same");
            }
            EXPECT_EQ(NEU_ERR_SUCCESS, checked(value));
            EXPECT_FALSE(was_changed);
            EXPECT_NE(0, read(true));
        }
    }
}

TEST_F(DriverCacheAuto, explicit_mapped_types_keep_legacy_sample_acceptance)
{
    for (neu_type_e alias : { NEU_TYPE_WORD, NEU_TYPE_DWORD, NEU_TYPE_LWORD }) {
        configure(alias);
        neu_dvalue_t value = {};
        value.type         = alias == NEU_TYPE_WORD
            ? NEU_TYPE_UINT16
            : alias == NEU_TYPE_DWORD ? NEU_TYPE_UINT32 : NEU_TYPE_UINT64;
        value.value.u64 = 42;
        EXPECT_EQ(NEU_ERR_SUCCESS, checked(value));
        ASSERT_EQ(0, read());
        EXPECT_EQ(value.type, out.value.type);
        EXPECT_EQ(alias, out.resolved_type);
    }
}
