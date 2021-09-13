#include <gtest/gtest.h>

#include "neu_data_expr.h"
#include "neu_types.h"

TEST(DataValueTest, neu_dvalue_set_int8)
{
    neu_data_val_t *val = neu_dvalue_new(NEU_DTYPE_INT8);
    int             rc  = neu_dvalue_set_int8(val, 16);
    EXPECT_EQ(0, rc);
    int8_t ret;
    rc = neu_dvalue_get_int8(val, &ret);
    EXPECT_EQ(0, rc);
    EXPECT_EQ(16, ret);
    neu_dvalue_free(val);
}

TEST(DataValueTest, neu_dvalue_set_int64)
{
    neu_data_val_t *val = neu_dvalue_new(NEU_DTYPE_INT64);
    int             rc  = neu_dvalue_set_int64(val, 100010111010);
    EXPECT_EQ(0, rc);
    int64_t ret;
    rc = neu_dvalue_get_int64(val, &ret);
    EXPECT_EQ(0, rc);
    EXPECT_EQ(100010111010, ret);
    neu_dvalue_free(val);
}

TEST(DataValueTest, neu_dvalue_set_double)
{
    neu_data_val_t *val = neu_dvalue_new(NEU_DTYPE_DOUBLE);
    int             rc  = neu_dvalue_set_double(val, 100010111010.010603);
    EXPECT_EQ(0, rc);
    double ret;
    rc = neu_dvalue_get_double(val, &ret);
    EXPECT_EQ(0, rc);
    EXPECT_EQ(100010111010.010603, ret);
    neu_dvalue_free(val);
}

TEST(DataValueTest, neu_dvalue_set_cstr)
{
    neu_data_val_t *val = neu_dvalue_new(NEU_DTYPE_CSTR);
    int             rc  = neu_dvalue_set_cstr(val, (char *) "Hello, Neuron!");
    EXPECT_EQ(0, rc);
    char *ret = NULL;
    rc        = neu_dvalue_get_cstr(val, &ret);
    EXPECT_EQ(0, rc);
    EXPECT_STREQ("Hello, Neuron!", ret);
    neu_dvalue_free(val);
}

TEST(DataValueTest, neu_dvalue_set_array)
{
    int             rc;
    neu_data_val_t *val =
        neu_dvalue_array_new(NEU_DTYPE_INT8, 4, sizeof(int8_t));
    neu_fixed_array_t *array_set;
    array_set = neu_fixed_array_new(4, sizeof(int8_t));
    rc        = neu_dvalue_set_array(val, array_set);
    neu_fixed_array_t *array_get;
    rc = neu_dvalue_get_array(val, &array_get);

    // TODO: assert two array is equal

    neu_fixed_array_free(array_set);
    neu_dvalue_free(val);
}

TEST(DataValueTest, neu_dvalue_set_vec)
{
    int             rc;
    neu_data_val_t *val = neu_dvalue_vec_new(NEU_DTYPE_INT8, 4, sizeof(int8_t));
    vector_t *      vec_set;
    vec_set = vector_new(4, sizeof(int8_t));
    rc      = neu_dvalue_set_vec(val, vec_set);
    vector_t *vec_get;
    rc = neu_dvalue_get_vec(val, &vec_get);

    // TODO: assert two vector is equal

    vector_free(vec_set);
    neu_dvalue_free(val);
}

TEST(DataValueTest, neu_dvalue_base_deser)
{
    int      rc;
    uint8_t *buf;

    neu_data_val_t *val = neu_dvalue_new(NEU_DTYPE_INT64);
    neu_dvalue_set_int64(val, 100);
    rc = neu_dvalue_serialize(val, &buf);
    EXPECT_EQ(0, rc);

    neu_data_val_t *val1;
    rc = neu_dvalue_desialize(buf, &val1);
    EXPECT_EQ(0, rc);
    int64_t i64;
    rc = neu_dvalue_get_int64(val1, &i64);
    EXPECT_EQ(0, rc);
    EXPECT_EQ(100, i64);

    free(buf);
    neu_dvalue_free(val);
    neu_dvalue_free(val1);
}

TEST(DataValueTest, neu_dvalue_array_deser)
{
    int      rc;
    uint8_t *buf;

    neu_data_val_t *val =
        neu_dvalue_array_new(NEU_DTYPE_INT8, 4, sizeof(int8_t));
    neu_fixed_array_t *array_set;
    array_set = neu_fixed_array_new(4, sizeof(int8_t));
    rc        = neu_dvalue_set_array(val, array_set);
    rc        = neu_dvalue_serialize(val, &buf);
    EXPECT_EQ(0, rc);

    neu_data_val_t *val1;
    rc = neu_dvalue_desialize(buf, &val1);
    EXPECT_EQ(0, rc);
    neu_fixed_array_t *array_get;
    rc = neu_dvalue_get_array(val, &array_get);
    EXPECT_EQ(0, rc);
    // TODO: assert two array is euqal

    free(buf);
    neu_fixed_array_free(array_set);
    neu_dvalue_free(val);
    neu_dvalue_free(val1);
}

TEST(DataValueTest, neu_dvalue_vec_deser)
{
    int      rc;
    uint8_t *buf;

    neu_data_val_t *val = neu_dvalue_vec_new(NEU_DTYPE_INT8, 4, sizeof(int8_t));
    vector_t *      vec_set;
    vec_set = vector_new(4, sizeof(int8_t));
    rc      = neu_dvalue_set_vec(val, vec_set);
    rc      = neu_dvalue_serialize(val, &buf);
    EXPECT_EQ(0, rc);

    neu_data_val_t *val1;
    rc = neu_dvalue_desialize(buf, &val1);
    EXPECT_EQ(0, rc);
    vector_t *vec_get;
    rc = neu_dvalue_get_vec(val, &vec_get);
    EXPECT_EQ(0, rc);
    // TODO: assert two vector is euqal

    free(buf);
    vector_free(vec_set);
    neu_dvalue_free(val);
    neu_dvalue_free(val1);
}
