#include <gtest/gtest.h>

extern "C" {
#include "adapter/driver/cache.h"

bool sub_filter_err = false;
}

class DriverCacheSample : public testing::Test {
protected:
    void SetUp() override
    {
        cache = neu_driver_cache_new();
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
        neu_dvalue_t value        = {};
        value.type                = NEU_TYPE_DOUBLE;
        value.value.d64           = 1.25;
        value.attrs.has_timestamp = true;
        value.attrs.timestamp     = 100;
        value.attrs.has_quality   = true;
        value.attrs.quality       = 192;
        return value;
    }

    bool update(neu_dvalue_t value)
    {
        return neu_driver_cache_update_change(cache, "group", "tag", 200,
                                              value, NULL, 0, false);
    }

    int changed()
    {
        return neu_driver_cache_meta_get_changed(cache, "group", "tag", &out,
                                                 &metas, &n_meta);
    }

    neu_driver_cache_t *cache = NULL;
    neu_driver_cache_value_t out = {};
    neu_tag_meta_t *metas = NULL;
    int n_meta = 0;
};

TEST_F(DriverCacheSample, existing_update_preserves_optional_attrs)
{
    for (int flags = 0; flags < 4; ++flags) {
        neu_dvalue_t value        = sample();
        value.attrs.has_timestamp = flags & 1;
        value.attrs.has_quality   = flags & 2;
        neu_driver_cache_update(cache, "group", "tag", 200, value, NULL, 0);

        ASSERT_EQ(0, neu_driver_cache_meta_get(cache, "group", "tag", &out,
                                               &metas, &n_meta));
        EXPECT_EQ(200, out.timestamp);
        EXPECT_EQ(value.attrs.has_timestamp, out.value.attrs.has_timestamp);
        EXPECT_EQ(value.attrs.has_quality, out.value.attrs.has_quality);
        EXPECT_EQ(flags & 1 ? 100 : 0, out.value.attrs.timestamp);
        EXPECT_EQ(flags & 2 ? 192 : 0, out.value.attrs.quality);
    }
}

TEST_F(DriverCacheSample, timestamp_alone_does_not_trigger_change)
{
    neu_dvalue_t value = sample();
    ASSERT_TRUE(update(value));
    ASSERT_EQ(0, changed());

    value.attrs.timestamp++;
    EXPECT_FALSE(update(value));
    EXPECT_NE(0, changed());
    ASSERT_EQ(0, neu_driver_cache_meta_get(cache, "group", "tag", &out,
                                           &metas, &n_meta));
    EXPECT_EQ(value.attrs.timestamp, out.value.attrs.timestamp);

    value.attrs.has_timestamp = false;
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
        EXPECT_FALSE(update(value));
        ASSERT_EQ(0, changed());
        EXPECT_TRUE(out.value.attrs.has_quality);
        EXPECT_EQ(0, out.value.attrs.quality);
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
    EXPECT_FALSE(out.value.attrs.has_timestamp);
    EXPECT_FALSE(out.value.attrs.has_quality);
    EXPECT_EQ(0, out.value.attrs.timestamp);
    EXPECT_EQ(0, out.value.attrs.quality);
    EXPECT_FALSE(update(value));
    EXPECT_NE(0, changed());
}

TEST_F(DriverCacheSample, forced_update_preserves_attrs)
{
    neu_dvalue_t value = sample();
    ASSERT_TRUE(update(value));
    ASSERT_EQ(0, changed());

    neu_driver_cache_update_change(cache, "group", "tag", 300, value, NULL, 0,
                                   true);
    ASSERT_EQ(0, changed());
    EXPECT_EQ(300, out.timestamp);
    EXPECT_EQ(value.attrs.timestamp, out.value.attrs.timestamp);
    EXPECT_EQ(value.attrs.quality, out.value.attrs.quality);
}

TEST_F(DriverCacheSample, filtered_error_compares_last_good_quality)
{
    sub_filter_err = true;
    neu_dvalue_t value = sample();
    ASSERT_TRUE(update(value));
    ASSERT_EQ(0, changed());

    neu_dvalue_t error = {};
    error.type         = NEU_TYPE_ERROR;
    error.value.i32    = 1;
    EXPECT_FALSE(update(error));
    EXPECT_NE(0, changed());
    EXPECT_FALSE(update(value));
    EXPECT_NE(0, changed());

    EXPECT_FALSE(update(error));
    value.attrs.quality = 0;
    EXPECT_TRUE(update(value));
    ASSERT_EQ(0, changed());
    EXPECT_TRUE(out.value.attrs.has_quality);
    EXPECT_EQ(0, out.value.attrs.quality);
}

TEST_F(DriverCacheSample, copy_dvalue_preserves_attrs)
{
    neu_dvalue_t src = sample();
    neu_dvalue_t dst = {};
    neu_copy_dvalue(&src, &dst);
    EXPECT_EQ(src.attrs.has_timestamp, dst.attrs.has_timestamp);
    EXPECT_EQ(src.attrs.timestamp, dst.attrs.timestamp);
    EXPECT_EQ(src.attrs.has_quality, dst.attrs.has_quality);
    EXPECT_EQ(src.attrs.quality, dst.attrs.quality);
}
