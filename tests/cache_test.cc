#include <gtest/gtest.h>

#include <neuron.h>

zlog_category_t *neuron = NULL;

TEST(CacheTest, neu_mem_cache_create)
{
    const size_t     max_bytes = 1024 * 1024;
    const size_t     max_items = 1024;
    neu_mem_cache_t *cache     = neu_mem_cache_create(max_bytes, max_items);
    EXPECT_NE(nullptr, cache);
    neu_mem_cache_destroy(cache);
}

TEST(CacheTest, neu_mem_cache_add)
{
    const size_t     max_bytes = 1024 * 1024;
    const size_t     max_items = 1024;
    neu_mem_cache_t *cache     = neu_mem_cache_create(max_bytes, max_items);
    EXPECT_NE(nullptr, cache);

    const char * str  = "hello,world!";
    cache_item_t item = {
        .size    = strlen(str) + 1,
        .data    = (void *) str,
        .release = NULL,
    };

    int rc = neu_mem_cache_add(cache, &item);
    EXPECT_EQ(0, rc);

    neu_mem_cache_destroy(cache);
}

TEST(CacheTest, neu_mem_cache_used)
{
    const size_t     max_bytes = 1024 * 1024;
    const size_t     max_items = 1024;
    neu_mem_cache_t *cache     = neu_mem_cache_create(max_bytes, max_items);
    EXPECT_NE(nullptr, cache);

    size_t used_bytes = 0;
    size_t used_msgs  = 0;
    neu_mem_cache_used(cache, &used_bytes, &used_msgs);
    EXPECT_EQ(0, used_bytes);
    EXPECT_EQ(0, used_msgs);

    const char * str  = "hello,world!";
    cache_item_t item = {
        .size    = strlen(str) + 1,
        .data    = (void *) str,
        .release = NULL,
    };

    int rc = neu_mem_cache_add(cache, &item);
    EXPECT_EQ(0, rc);

    neu_mem_cache_used(cache, &used_bytes, &used_msgs);
    EXPECT_EQ(strlen(str) + 1, used_bytes);
    EXPECT_EQ(1, used_msgs);

    cache_item_t item1 = neu_mem_cache_earliest(cache);
    EXPECT_NE(nullptr, item1.data);
    EXPECT_STREQ((char *) item1.data, str);
    EXPECT_EQ(item1.size, strlen(str) + 1);

    neu_mem_cache_used(cache, &used_bytes, &used_msgs);
    EXPECT_EQ(0, used_bytes);
    EXPECT_EQ(0, used_msgs);

    neu_mem_cache_destroy(cache);
}

TEST(CacheTest, neu_mem_cache_latest)
{
    const size_t     max_bytes = 1024 * 1024;
    const size_t     max_items = 1024;
    neu_mem_cache_t *cache     = neu_mem_cache_create(max_bytes, max_items);
    EXPECT_NE(nullptr, cache);

    size_t used_bytes = 0;
    size_t used_items = 0;
    neu_mem_cache_used(cache, &used_bytes, &used_items);
    EXPECT_EQ(0, used_bytes);
    EXPECT_EQ(0, used_items);

    const char * str  = "hello,world!";
    cache_item_t item = {
        .size    = strlen(str) + 1,
        .data    = (void *) str,
        .release = NULL,
    };

    int rc = neu_mem_cache_add(cache, &item);
    EXPECT_EQ(0, rc);

    neu_mem_cache_used(cache, &used_bytes, &used_items);
    EXPECT_EQ(strlen(str) + 1, used_bytes);
    EXPECT_EQ(1, used_items);

    cache_item_t item1 = neu_mem_cache_latest(cache);
    EXPECT_NE(nullptr, item1.data);
    EXPECT_STREQ((char *) item1.data, str);
    EXPECT_EQ(item1.size, strlen(str) + 1);

    neu_mem_cache_used(cache, &used_bytes, &used_items);
    EXPECT_EQ(0, used_bytes);
    EXPECT_EQ(0, used_items);

    neu_mem_cache_destroy(cache);
}

int main(int argc, char **argv)
{
    zlog_init("./config/dev.conf");
    neuron = zlog_get_category("neuron");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
