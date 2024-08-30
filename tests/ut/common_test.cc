#include <gtest/gtest.h>

#include "neuron.h"
#include "type.h"

zlog_category_t *neuron = NULL;
TEST(type_set_get, type_set)
{
    uint16_t u16 = 0;
    neu_set_u16((uint8_t *) &u16, 0x1234);
    EXPECT_EQ(0x1234, u16);

    int16_t i16 = 0;
    neu_set_i16((uint8_t *) &i16, 0x1234);
    EXPECT_EQ(0x1234, i16);

    uint32_t u32 = 0;
    neu_set_u32((uint8_t *) &u32, 0x12345678);
    EXPECT_EQ(0x12345678, u32);

    int32_t i32 = 0;
    neu_set_i32((uint8_t *) &i32, 0x12345678);
    EXPECT_EQ(0x12345678, i32);

    float f32 = 0;
    neu_set_f32((uint8_t *) &f32, 3.1415926);
    EXPECT_FLOAT_EQ(3.1415926, f32);

    double f64 = 0;
    neu_set_f64((uint8_t *) &f64, 3.1415926);
    EXPECT_DOUBLE_EQ(3.1415926, f64);

    int64_t i64 = 0;
    neu_set_i64((uint8_t *) &i64, 0x1234567890ABCDEF);
    EXPECT_EQ(0x1234567890ABCDEF, i64);

    uint64_t u64 = 0;
    neu_set_u64((uint8_t *) &u64, 0x1234567890ABCDEF);
    EXPECT_EQ(0x1234567890ABCDEF, u64);
}

TEST(type_set_get, type_get)
{
    uint16_t u16 = 0x1234;
    EXPECT_EQ(0x1234, neu_get_u16((uint8_t *) &u16));

    int16_t i16_ = 0x1234;
    EXPECT_EQ(0x1234, neu_get_i16((uint8_t *) &i16_));

    uint32_t u32 = 0x12345678;
    EXPECT_EQ(0x12345678, neu_get_u32((uint8_t *) &u32));

    int32_t i32 = 0x12345678;
    EXPECT_EQ(0x12345678, neu_get_i32((uint8_t *) &i32));

    float f32 = 3.1415926;
    EXPECT_FLOAT_EQ(3.1415926, neu_get_f32((uint8_t *) &f32));

    double f64 = 3.1415926;
    EXPECT_DOUBLE_EQ(3.1415926, neu_get_f64((uint8_t *) &f64));

    int64_t i64 = 0x1234567890ABCDEF;
    EXPECT_EQ(0x1234567890ABCDEF, neu_get_i64((uint8_t *) &i64));

    uint64_t u64 = 0x1234567890ABCDEF;
    EXPECT_EQ(0x1234567890ABCDEF, neu_get_u64((uint8_t *) &u64));
}

TEST(type_h_n, type_h_n)
{
    uint16_t u16 = 0x1234;
    neu_htons_p(&u16);
    EXPECT_EQ(0x3412, u16);

    uint32_t u32 = 0x12345678;
    neu_htonl_p(&u32);
    EXPECT_EQ(0x78563412, u32);

    uint64_t u64 = 0x1234567890ABCDEF;
    neu_htonll_p(&u64);
    EXPECT_EQ(0xEFCDAB9078563412, u64);
}

int main(int argc, char **argv)
{
    zlog_init("./config/dev.conf");
    neuron = zlog_get_category("neuron");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
