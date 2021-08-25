#include <gtest/gtest.h>

#include "neu_types.h"

TEST(VariableTest, neu_variable_create)
{
    neu_variable_t *v = neu_variable_create();
    EXPECT_NE(v, nullptr);
    neu_variable_destroy(v);
}

TEST(VariableTest, neu_variable_count)
{
    neu_variable_t *v     = neu_variable_create();
    size_t          count = neu_variable_count(v);
    EXPECT_EQ(1, count);
    neu_variable_destroy(v);
}

TEST(VariableTest, neu_variable_set_byte)
{
    neu_variable_t *v  = neu_variable_create();
    int             rc = neu_variable_set_byte(v, 16);
    EXPECT_EQ(0, rc);
    int8_t ret;
    rc = neu_variable_get_byte(v, &ret);
    EXPECT_EQ(0, rc);
    EXPECT_EQ(16, ret);
    neu_variable_destroy(v);
}

TEST(VariableTest, neu_variable_set_qword)
{
    neu_variable_t *v  = neu_variable_create();
    int             rc = neu_variable_set_qword(v, 100010111010);
    EXPECT_EQ(0, rc);
    int64_t ret;
    rc = neu_variable_get_qword(v, &ret);
    EXPECT_EQ(0, rc);
    EXPECT_EQ(100010111010, ret);
    neu_variable_destroy(v);
}

TEST(VariableTest, neu_variable_set_double)
{
    neu_variable_t *v  = neu_variable_create();
    int             rc = neu_variable_set_double(v, 100010111010.010603);
    EXPECT_EQ(0, rc);
    double ret;
    rc = neu_variable_get_double(v, &ret);
    EXPECT_EQ(0, rc);
    EXPECT_EQ(100010111010.010603, ret);
    neu_variable_destroy(v);
}

TEST(VariableTest, neu_variable_set_string)
{
    neu_variable_t *v  = neu_variable_create();
    int             rc = neu_variable_set_string(v, "Hello, Neuron!");
    EXPECT_EQ(0, rc);
    char * ret = NULL;
    size_t len;
    rc = neu_variable_get_string(v, &ret, &len);
    EXPECT_EQ(0, rc);
    EXPECT_STREQ("Hello, Neuron!", ret);
    neu_variable_destroy(v);
}

TEST(VariableTest, neu_variable_add_item)
{
    neu_variable_t *v  = neu_variable_create();
    neu_variable_t *i0 = neu_variable_create();
    neu_variable_t *i1 = neu_variable_create();
    neu_variable_t *i2 = neu_variable_create();
    neu_variable_t *i3 = neu_variable_create();
    neu_variable_t *i4 = neu_variable_create();

    EXPECT_EQ(0, neu_variable_add_item(v, i0));
    EXPECT_EQ(0, neu_variable_add_item(v, i1));
    EXPECT_EQ(0, neu_variable_add_item(v, i2));
    EXPECT_EQ(0, neu_variable_add_item(v, i3));
    EXPECT_EQ(0, neu_variable_add_item(v, i4));

    size_t count = neu_variable_count(v);
    EXPECT_EQ(6, count);
    neu_variable_destroy(v);
}

TEST(VariableTest, neu_variable_get_item)
{
    neu_variable_t *v = neu_variable_create();

    neu_variable_t *i0 = neu_variable_create();
    neu_variable_set_qword(i0, 100);
    neu_variable_set_error(i0, 0);

    neu_variable_t *i1 = neu_variable_create();
    neu_variable_set_double(i1, 100010111010.010603);
    neu_variable_set_error(i1, 0);

    neu_variable_t *i2 = neu_variable_create();
    neu_variable_set_string(i2, "Read Error");
    neu_variable_set_error(i2, 10);

    EXPECT_EQ(0, neu_variable_add_item(v, i0));
    EXPECT_EQ(0, neu_variable_add_item(v, i1));
    EXPECT_EQ(0, neu_variable_add_item(v, i2));

    size_t count = neu_variable_count(v);
    EXPECT_EQ(4, count);

    neu_variable_t *ii0 = NULL;
    neu_variable_t *ii1 = NULL;
    neu_variable_t *ii2 = NULL;
    int             rc  = neu_variable_get_item(v, 1, &ii0);
    EXPECT_EQ(0, rc);
    int64_t ret_qword;
    rc = neu_variable_get_qword(ii0, &ret_qword);
    EXPECT_EQ(0, rc);
    EXPECT_EQ(100, ret_qword);

    rc = neu_variable_get_item(v, 2, &ii1);
    EXPECT_EQ(0, rc);
    double ret_double;
    rc = neu_variable_get_double(ii1, &ret_double);
    EXPECT_EQ(0, rc);
    EXPECT_EQ(100010111010.010603, ret_double);

    rc = neu_variable_get_item(v, 3, &ii2);
    EXPECT_EQ(0, rc);
    char * ret_string = NULL;
    size_t len;
    rc = neu_variable_get_string(ii2, &ret_string, &len);
    EXPECT_EQ(0, rc);
    EXPECT_STREQ("Read Error", ret_string);

    neu_variable_destroy(v);
}

TEST(VariableTest, neu_variable_serialize)
{
    neu_variable_t *v = neu_variable_create();
    neu_variable_set_qword(v, 100);
    neu_variable_set_error(v, 0);

    void * buf;
    size_t len = neu_variable_serialize(v, &buf);
    EXPECT_EQ(24, len);
    neu_variable_serialize_free(buf);

    buf                = NULL;
    neu_variable_t *i2 = neu_variable_create();
    neu_variable_set_string(i2, "Read Error.");
    neu_variable_set_error(i2, 10);
    neu_variable_add_item(v, i2);
    len = neu_variable_serialize(v, &buf);
    EXPECT_EQ(52, len);
    neu_variable_serialize_free(buf);

    buf                = NULL;
    neu_variable_t *i1 = neu_variable_create();
    neu_variable_set_double(i1, 100010111010.010603);
    neu_variable_set_error(i1, 0);
    neu_variable_add_item(v, i1);
    len = neu_variable_serialize(v, &buf);
    EXPECT_EQ(76, len);
    neu_variable_serialize_free(buf);

    neu_variable_destroy(v);
}

TEST(VariableTest, neu_variable_deserialize)
{
    neu_variable_t *v = neu_variable_create();
    neu_variable_set_qword(v, 100);
    neu_variable_set_error(v, 0);

    neu_variable_t *i1 = neu_variable_create();
    neu_variable_set_string(i1, "Read Error.");
    neu_variable_set_error(i1, 10);
    neu_variable_add_item(v, i1);

    neu_variable_t *i2 = neu_variable_create();
    neu_variable_set_double(i2, 100010111010.010603);
    neu_variable_set_error(i2, 0);
    neu_variable_add_item(v, i2);

    void * buf;
    size_t len = neu_variable_serialize(v, &buf);
    EXPECT_EQ(76, len);

    neu_variable_t *vv = neu_variable_deserialize(buf, len);
    EXPECT_NE(nullptr, vv);
    size_t count = neu_variable_count(vv);
    EXPECT_EQ(3, count);

    neu_variable_t *ii0 = NULL;
    neu_variable_t *ii1 = NULL;
    neu_variable_t *ii2 = NULL;
    int             rc  = neu_variable_get_item(v, 0, &ii0);
    EXPECT_EQ(0, rc);
    int64_t ret_qword;
    rc = neu_variable_get_qword(ii0, &ret_qword);
    EXPECT_EQ(0, rc);
    EXPECT_EQ(100, ret_qword);

    rc = neu_variable_get_item(v, 1, &ii1);
    EXPECT_EQ(0, rc);
    char * ret_string = NULL;
    size_t len_string;
    rc = neu_variable_get_string(ii1, &ret_string, &len_string);
    EXPECT_EQ(0, rc);
    EXPECT_STREQ("Read Error.", ret_string);

    rc = neu_variable_get_item(v, 2, &ii2);
    EXPECT_EQ(0, rc);
    double ret_double;
    rc = neu_variable_get_double(ii2, &ret_double);
    EXPECT_EQ(0, rc);
    EXPECT_EQ(100010111010.010603, ret_double);

    neu_variable_destroy(vv);
    neu_variable_serialize_free(buf);
    neu_variable_destroy(v);
}