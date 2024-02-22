#include <gtest/gtest.h>

extern "C" {
#include "utils/async_queue.h"
#include "utils/log.h"
}

zlog_category_t *neuron = NULL;

uint64_t mock_key_fn(void *data)
{
    return *reinterpret_cast<uint64_t *>(data);
}

bool mock_expire_fn(void *data)
{
    return false;
}

void mock_free_fn(void *data)
{
    delete reinterpret_cast<uint64_t *>(data);
}

bool mock_filter(void *filter_data, void *data)
{
    return *reinterpret_cast<uint64_t *>(filter_data) ==
        *reinterpret_cast<uint64_t *>(data);
}

void *create_test_data(uint64_t value)
{
    uint64_t *data = new uint64_t(value);
    return static_cast<void *>(data);
}

TEST(neu_async_queue_test, create_destroy)
{
    auto q = neu_async_queue_new(mock_key_fn, mock_expire_fn, mock_free_fn, 10);
    ASSERT_TRUE(q != nullptr);
    neu_async_queue_destroy(q);
}

TEST(neu_async_queue_test, push)
{
    const int max_size = 3;
    auto      q = neu_async_queue_new(mock_key_fn, mock_expire_fn, mock_free_fn,
                                 max_size);

    for (int i = 0; i < max_size; ++i) {
        neu_async_queue_push(q, create_test_data(i));
    }

    neu_async_queue_push(q, create_test_data(max_size));

    neu_async_queue_destroy(q);
}

TEST(neu_async_queue_test, pop)
{
    auto q = neu_async_queue_new(mock_key_fn, mock_expire_fn, mock_free_fn, 10);

    auto data = create_test_data(42);
    neu_async_queue_push(q, data);

    void *popped_data = nullptr;
    auto  result      = neu_async_queue_pop(q, mock_key_fn(data), &popped_data);

    ASSERT_EQ(result, 0);
    ASSERT_EQ(popped_data, data);

    mock_free_fn(data);
    neu_async_queue_destroy(q);
}

TEST(neu_async_queue_test, remove)
{
    auto q = neu_async_queue_new(mock_key_fn, mock_expire_fn, mock_free_fn, 10);

    for (int i = 0; i < 5; ++i) {
        neu_async_queue_push(q, create_test_data(i));
    }

    uint64_t remove_key = 2;
    neu_async_queue_remove(q, mock_filter, &remove_key);

    void *popped_data = nullptr;
    auto  pop_result  = neu_async_queue_pop(q, remove_key, &popped_data);
    ASSERT_EQ(pop_result, -1);

    neu_async_queue_destroy(q);
}

TEST(neu_async_queue_test, clean)
{
    auto q = neu_async_queue_new(mock_key_fn, mock_expire_fn, mock_free_fn, 10);

    for (int i = 0; i < 5; ++i) {
        neu_async_queue_push(q, create_test_data(i));
    }

    neu_async_queue_clean(q);

    void *popped_data = nullptr;
    auto  pop_result  = neu_async_queue_pop(q, 0, &popped_data);
    ASSERT_EQ(pop_result, -1);

    neu_async_queue_destroy(q);
}

int main(int argc, char **argv)
{
    zlog_init("./config/dev.conf");
    neuron = zlog_get_category("neuron");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}