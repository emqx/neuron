#include <gtest/gtest.h>

#include "neu_data_expr.h"
#include "neu_types.h"

TEST(DataValueTest, neu_dvalue_init_set_get_prim_val)
{
    neu_data_val_t *val;

    uint8_t ret_bit;
    int8_t  ret_int8;
    int16_t ret_int16;
    int32_t ret_int32;
    int64_t ret_int64;

    uint8_t  ret_uint8;
    uint16_t ret_uint16;
    uint32_t ret_uint32;
    uint64_t ret_uint64;

    float  ret_float;
    double ret_double;
    bool   b;

    /** test bit **/
    val = neu_dvalue_new(NEU_DTYPE_BIT);
    neu_dvalue_init_bit(val, 1);
    EXPECT_EQ(0, neu_dvalue_get_bit(val, &ret_bit));
    EXPECT_EQ(1, ret_bit);

    neu_dvalue_set_bit(val, 0);
    EXPECT_EQ(0, neu_dvalue_get_bit(val, &ret_bit));
    EXPECT_EQ(0, ret_bit);
    neu_dvalue_free(val);

    /** test bool **/
    val = neu_dvalue_new(NEU_DTYPE_BOOL);
    neu_dvalue_init_bool(val, true);
    EXPECT_EQ(0, neu_dvalue_get_bool(val, &b));
    EXPECT_EQ(true, b);

    neu_dvalue_set_bool(val, false);
    EXPECT_EQ(0, neu_dvalue_get_bool(val, &b));
    EXPECT_EQ(false, b);
    neu_dvalue_free(val);

    /** test int8 **/
    val = neu_dvalue_new(NEU_DTYPE_INT8);
    neu_dvalue_init_int8(val, 16);
    EXPECT_EQ(0, neu_dvalue_get_int8(val, &ret_int8));
    EXPECT_EQ(16, ret_int8);

    neu_dvalue_set_int8(val, 17);
    EXPECT_EQ(0, neu_dvalue_get_int8(val, &ret_int8));
    EXPECT_EQ(17, ret_int8);
    neu_dvalue_free(val);

    /** test int16 **/
    val = neu_dvalue_new(NEU_DTYPE_INT16);
    neu_dvalue_init_int16(val, 4096);
    EXPECT_EQ(0, neu_dvalue_get_int16(val, &ret_int16));
    EXPECT_EQ(4096, ret_int16);

    neu_dvalue_set_int16(val, 32767);
    EXPECT_EQ(0, neu_dvalue_get_int16(val, &ret_int16));
    EXPECT_EQ(32767, ret_int16);
    neu_dvalue_free(val);

    /** test int32 **/
    val = neu_dvalue_new(NEU_DTYPE_INT32);
    neu_dvalue_init_int32(val, 268435456);
    EXPECT_EQ(0, neu_dvalue_get_int32(val, &ret_int32));
    EXPECT_EQ(268435456, ret_int32);

    neu_dvalue_set_int32(val, 268435457);
    EXPECT_EQ(0, neu_dvalue_get_int32(val, &ret_int32));
    EXPECT_EQ(268435457, ret_int32);
    neu_dvalue_free(val);

    /** test int64 **/
    val = neu_dvalue_new(NEU_DTYPE_INT64);
    neu_dvalue_init_int64(val, 100010111010);
    EXPECT_EQ(0, neu_dvalue_get_int64(val, &ret_int64));
    EXPECT_EQ(100010111010, ret_int64);

    neu_dvalue_set_int64(val, 100010111011);
    EXPECT_EQ(0, neu_dvalue_get_int64(val, &ret_int64));
    EXPECT_EQ(100010111011, ret_int64);
    neu_dvalue_free(val);

    /** test uint8**/
    val = neu_dvalue_new(NEU_DTYPE_UINT8);
    neu_dvalue_init_uint8(val, 16);
    EXPECT_EQ(0, neu_dvalue_get_uint8(val, &ret_uint8));
    EXPECT_EQ(16, ret_uint8);

    neu_dvalue_set_uint8(val, 17);
    EXPECT_EQ(0, neu_dvalue_get_uint8(val, &ret_uint8));
    EXPECT_EQ(17, ret_uint8);
    neu_dvalue_free(val);

    /** test uint16**/
    val = neu_dvalue_new(NEU_DTYPE_UINT16);
    neu_dvalue_init_uint16(val, 4096);
    EXPECT_EQ(0, neu_dvalue_get_uint16(val, &ret_uint16));
    EXPECT_EQ(4096, ret_uint16);

    neu_dvalue_set_uint16(val, 65535);
    EXPECT_EQ(0, neu_dvalue_get_uint16(val, &ret_uint16));
    EXPECT_EQ(65535, ret_uint16);
    neu_dvalue_free(val);

    /** test uint32 **/
    val = neu_dvalue_new(NEU_DTYPE_UINT32);
    neu_dvalue_init_uint32(val, 268435456);
    EXPECT_EQ(0, neu_dvalue_get_uint32(val, &ret_uint32));
    EXPECT_EQ(268435456, ret_uint32);

    neu_dvalue_set_uint32(val, 268435457);
    EXPECT_EQ(0, neu_dvalue_get_uint32(val, &ret_uint32));
    EXPECT_EQ(268435457, ret_uint32);
    neu_dvalue_free(val);

    /** test uint64 **/
    val = neu_dvalue_new(NEU_DTYPE_UINT64);
    neu_dvalue_init_uint64(val, 100010111010);
    EXPECT_EQ(0, neu_dvalue_get_uint64(val, &ret_uint64));
    EXPECT_EQ(100010111010, ret_uint64);

    neu_dvalue_set_uint64(val, 100010111011);
    EXPECT_EQ(0, neu_dvalue_get_uint64(val, &ret_uint64));
    EXPECT_EQ(100010111011, ret_uint64);
    neu_dvalue_free(val);

    /** test float **/
    val = neu_dvalue_new(NEU_DTYPE_FLOAT);
    neu_dvalue_init_float(val, (float) 100.101);
    EXPECT_EQ(0, neu_dvalue_get_float(val, &ret_float));
    EXPECT_EQ((float) 100.101, ret_float);

    neu_dvalue_set_float(val, (float) 101.101);
    EXPECT_EQ(0, neu_dvalue_get_float(val, &ret_float));
    EXPECT_EQ((float) 101.101, ret_float);
    neu_dvalue_free(val);

    /** test double **/
    val = neu_dvalue_new(NEU_DTYPE_DOUBLE);
    neu_dvalue_init_double(val, 100010111010.010603);
    EXPECT_EQ(0, neu_dvalue_get_double(val, &ret_double));
    EXPECT_EQ(100010111010.010603, ret_double);

    neu_dvalue_set_double(val, 100010111010.010604);
    EXPECT_EQ(0, neu_dvalue_get_double(val, &ret_double));
    EXPECT_EQ(100010111010.010604, ret_double);
    neu_dvalue_free(val);

    /** test errorcode **/
    int32_t ret_error = 0;
    val               = neu_dvalue_new(NEU_DTYPE_ERRORCODE);
    neu_dvalue_init_errorcode(val, -1);
    EXPECT_EQ(0, neu_dvalue_get_errorcode(val, &ret_error));
    EXPECT_EQ(NEU_DTYPE_ERRORCODE, neu_dvalue_get_type(val));
    EXPECT_EQ(-1, ret_error);

    EXPECT_EQ(0, neu_dvalue_set_errorcode(val, 0));
    EXPECT_EQ(0, neu_dvalue_get_errorcode(val, &ret_error));
    EXPECT_EQ(0, ret_error);

    EXPECT_EQ(0, neu_dvalue_set_errorcode(val, 1));
    EXPECT_EQ(0, neu_dvalue_get_errorcode(val, &ret_error));
    EXPECT_EQ(1, ret_error);
    neu_dvalue_free(val);

    val = neu_dvalue_new(NEU_DTYPE_ERRORCODE);
    EXPECT_EQ(NEU_DTYPE_ERRORCODE, neu_dvalue_get_type(val));
    EXPECT_EQ(0, neu_dvalue_set_errorcode(val, 2));
    EXPECT_EQ(0, neu_dvalue_get_errorcode(val, &ret_error));
    EXPECT_EQ(2, ret_error);
    neu_dvalue_free(val);
}

#define INT_VAL_TEST_STR "hello int-val"
#define STRING_KEY_TEST_STR "string-key"
#define STRING_VAL_TEST_STR "hello string-val"
TEST(DataValueTest, neu_dvalue_set_get_keyvalue)
{
    /* teset set/get int_val */
    neu_int_val_t   int_val;
    neu_data_val_t *input_val;
    input_val = neu_dvalue_new(NEU_DTYPE_CSTR);
    neu_dvalue_set_cstr(input_val, (char *) INT_VAL_TEST_STR);
    neu_int_val_init(&int_val, 21, input_val);

    neu_data_val_t *val;
    int             rc;
    val = neu_dvalue_new(NEU_DTYPE_INT_VAL);
    rc  = neu_dvalue_set_int_val(val, int_val);
    EXPECT_EQ(0, rc);

    neu_int_val_t int_val_get;
    char *        cstr;
    rc = neu_dvalue_get_int_val(val, &int_val_get);
    EXPECT_EQ(0, rc);
    rc = neu_dvalue_get_ref_cstr(int_val_get.val, &cstr);
    EXPECT_EQ(0, rc);
    EXPECT_STREQ((char *) INT_VAL_TEST_STR, cstr);
    neu_int_val_uninit(&int_val_get);
    neu_dvalue_free(val);

    /* teset set/get string_val */
    neu_string_t *input_key;
    neu_string_t *test_string;
    // neu_data_val_t* input_val;
    neu_string_val_t string_val;
    input_key   = neu_string_from_cstr((char *) STRING_KEY_TEST_STR);
    test_string = neu_string_from_cstr((char *) STRING_VAL_TEST_STR);
    input_val   = neu_dvalue_new(NEU_DTYPE_STRING);
    neu_dvalue_set_move_string(input_val, test_string);
    neu_string_val_init(&string_val, input_key, input_val);

    val = neu_dvalue_new(NEU_DTYPE_STRING_VAL);
    rc  = neu_dvalue_set_string_val(val, string_val);
    EXPECT_EQ(0, rc);

    neu_string_val_t string_val_get;
    char *           key_cstr;
    char *           val_cstr;
    neu_string_t *   string_get;
    rc = neu_dvalue_get_string_val(val, &string_val_get);
    EXPECT_EQ(0, rc);
    key_cstr = neu_string_get_ref_cstr(string_val_get.key);
    EXPECT_STREQ((char *) STRING_KEY_TEST_STR, key_cstr);
    rc = neu_dvalue_get_ref_string(string_val_get.val, &string_get);
    EXPECT_EQ(0, rc);
    EXPECT_STREQ((char *) STRING_VAL_TEST_STR,
                 neu_string_get_ref_cstr(string_get));
    neu_string_val_uninit(&string_val_get);
    neu_dvalue_free(val);
}

TEST(DataValueTest, neu_dvalue_set_get_cstr)
{
    neu_data_val_t *val = neu_dvalue_new(NEU_DTYPE_CSTR);
    int             rc  = neu_dvalue_set_cstr(val, (char *) "Hello, Neuron!");
    EXPECT_EQ(0, rc);
    char *ret = NULL;
    rc        = neu_dvalue_get_cstr(val, &ret);
    EXPECT_EQ(0, rc);
    EXPECT_STREQ("Hello, Neuron!", ret);
    free(ret);
    neu_dvalue_free(val);
}

TEST(DataValueTest, neu_dvalue_set_get_string)
{
    neu_data_val_t *val        = neu_dvalue_new(NEU_DTYPE_STRING);
    neu_string_t *  string_set = NULL;

    // To do

    neu_dvalue_free(val);
}

TEST(DataValueTest, neu_dvalue_set_get_bytes)
{
    neu_data_val_t *val = neu_dvalue_new(NEU_DTYPE_BYTES);
    neu_bytes_t *   bytes_set;
    neu_bytes_t *   bytes_get;
    int             rc;

    bytes_set = neu_bytes_new(16);
    rc        = neu_dvalue_set_bytes(val, bytes_set);
    EXPECT_EQ(0, rc);
    rc = neu_dvalue_get_bytes(val, &bytes_get);
    EXPECT_EQ(0, rc);

    size_t bytes_length = bytes_set->length;
    EXPECT_EQ(0, memcmp(bytes_set->buf, bytes_get->buf, bytes_length));
    EXPECT_EQ(bytes_set->length, bytes_get->length);

    neu_bytes_free(bytes_set);
    neu_bytes_free(bytes_get);
    neu_dvalue_free(val);
}

TEST(DataValueTest, neu_dvalue_set_get_array)
{
    int                rc;
    neu_fixed_array_t *array_set;
    neu_fixed_array_t *array_get;
    neu_fixed_array_t *array_get_ref;

    int8_t i8_arr[4] = { 2, 7, 1, 8 };
    array_set        = neu_fixed_array_new(4, sizeof(int8_t));
    neu_fixed_array_set(array_set, 0, &i8_arr[0]);
    neu_fixed_array_set(array_set, 1, &i8_arr[1]);
    neu_fixed_array_set(array_set, 2, &i8_arr[2]);
    neu_fixed_array_set(array_set, 3, &i8_arr[3]);

    neu_data_val_t *val = neu_dvalue_unit_new();
    neu_dvalue_init_move_array(val, NEU_DTYPE_INT8, array_set);

    size_t array_length = array_set->length;
    neu_dvalue_get_ref_array(val, &array_get_ref);
    EXPECT_EQ(0, memcmp(array_set->buf, array_get_ref->buf, array_length));
    EXPECT_EQ(array_set->esize, array_get_ref->esize);
    EXPECT_EQ(array_set->length, array_get_ref->length);

    rc = neu_dvalue_get_move_array(val, &array_get);
    EXPECT_EQ(0, rc);

    EXPECT_EQ(0, memcmp(array_set->buf, array_get->buf, array_length));
    EXPECT_EQ(array_set->esize, array_get->esize);
    EXPECT_EQ(array_set->length, array_get->length);

    neu_fixed_array_free(array_get);
    neu_dvalue_free(val);
}

TEST(DataValueTest, neu_dvalue_set_get_vec)
{
    int       rc;
    vector_t *vec_set;
    int8_t    i8_arr[4] = { 2, 7, 1, 8 };
    vec_set             = vector_new(4, sizeof(int8_t));
    vector_push_back(vec_set, &i8_arr[0]);
    vector_push_back(vec_set, &i8_arr[1]);
    vector_push_back(vec_set, &i8_arr[2]);
    vector_push_back(vec_set, &i8_arr[3]);

    neu_data_val_t *val = neu_dvalue_unit_new();
    neu_dvalue_init_move_vec(val, NEU_DTYPE_INT8, vec_set);
    vector_t *vec_get;
    rc = neu_dvalue_get_move_vec(val, &vec_get);

    // TODO: assert two vector is equal

    vector_free(vec_get);
    neu_dvalue_free(val);
}

TEST(DataValueTest, neu_dvalue_prim_val_deser)
{
    ssize_t  size;
    uint8_t *buf = NULL;

    neu_data_val_t *val  = NULL;
    neu_data_val_t *val1 = NULL;

    /** bit serialize and deserialize test **/
    val = neu_dvalue_new(NEU_DTYPE_BIT);
    EXPECT_EQ(0, neu_dvalue_set_bit(val, 1));
    size = neu_dvalue_serialize(val, &buf);
    EXPECT_LT(0, size);
    neu_dvalue_free(val);

    size = neu_dvalue_deserialize(buf, size, &val1);
    EXPECT_LT(0, size);
    uint8_t bit;
    EXPECT_EQ(0, neu_dvalue_get_bit(val1, &bit));
    EXPECT_EQ(1, bit);

    free(buf);
    neu_dvalue_free(val1);

    /** bool serialize and deserialize test **/
    val = neu_dvalue_new(NEU_DTYPE_BOOL);
    EXPECT_EQ(0, neu_dvalue_set_bool(val, true));
    size = neu_dvalue_serialize(val, &buf);
    EXPECT_LT(0, size);
    neu_dvalue_free(val);

    size = neu_dvalue_deserialize(buf, size, &val1);
    EXPECT_LT(0, size);
    bool b;
    EXPECT_EQ(0, neu_dvalue_get_bool(val1, &b));
    EXPECT_EQ(true, b);

    free(buf);
    neu_dvalue_free(val1);

    /** int8 serialize and deserialize test **/
    val = neu_dvalue_new(NEU_DTYPE_INT8);
    EXPECT_EQ(0, neu_dvalue_set_int8(val, 8));
    size = neu_dvalue_serialize(val, &buf);
    EXPECT_LT(0, size);
    neu_dvalue_free(val);

    size = neu_dvalue_deserialize(buf, size, &val1);
    EXPECT_LT(0, size);
    int8_t i8;
    EXPECT_EQ(0, neu_dvalue_get_int8(val1, &i8));
    EXPECT_EQ(8, i8);

    free(buf);
    neu_dvalue_free(val1);

    /** int16 serialize and deserialize test **/
    val = neu_dvalue_new(NEU_DTYPE_INT16);
    EXPECT_EQ(0, neu_dvalue_set_int16(val, 16));
    size = neu_dvalue_serialize(val, &buf);
    EXPECT_LT(0, size);
    neu_dvalue_free(val);

    size = neu_dvalue_deserialize(buf, size, &val1);
    EXPECT_LT(0, size);
    int16_t i16;
    EXPECT_EQ(0, neu_dvalue_get_int16(val1, &i16));
    EXPECT_EQ(16, i16);

    free(buf);
    neu_dvalue_free(val1);

    /** int32 serialize and deserialize test **/
    val = neu_dvalue_new(NEU_DTYPE_INT32);
    EXPECT_EQ(0, neu_dvalue_set_int32(val, 32));
    size = neu_dvalue_serialize(val, &buf);
    EXPECT_LT(0, size);
    neu_dvalue_free(val);

    size = neu_dvalue_deserialize(buf, size, &val1);
    EXPECT_LT(0, size);
    int32_t i32;
    EXPECT_EQ(0, neu_dvalue_get_int32(val1, &i32));
    EXPECT_EQ(32, i32);

    free(buf);
    neu_dvalue_free(val1);

    /** int64 serialize and deserialize test **/
    val = neu_dvalue_new(NEU_DTYPE_INT64);
    EXPECT_EQ(0, neu_dvalue_set_int64(val, 100));
    size = neu_dvalue_serialize(val, &buf);
    EXPECT_LT(0, size);
    neu_dvalue_free(val);

    size = neu_dvalue_deserialize(buf, size, &val1);
    EXPECT_LT(0, size);
    int64_t i64;
    EXPECT_EQ(0, neu_dvalue_get_int64(val1, &i64));
    EXPECT_EQ(100, i64);

    free(buf);
    neu_dvalue_free(val1);

    /** uint8 serialize and deserialize test **/
    val = neu_dvalue_new(NEU_DTYPE_UINT8);
    EXPECT_EQ(0, neu_dvalue_set_uint8(val, 8));
    size = neu_dvalue_serialize(val, &buf);
    EXPECT_LT(0, size);
    neu_dvalue_free(val);

    size = neu_dvalue_deserialize(buf, size, &val1);
    EXPECT_LT(0, size);
    uint8_t ui8;
    EXPECT_EQ(0, neu_dvalue_get_uint8(val1, &ui8));
    EXPECT_EQ(8, ui8);

    free(buf);
    neu_dvalue_free(val1);

    /** uint16 serialize and deserialize test **/
    val = neu_dvalue_new(NEU_DTYPE_UINT16);
    EXPECT_EQ(0, neu_dvalue_set_uint16(val, 16));
    size = neu_dvalue_serialize(val, &buf);
    EXPECT_LT(0, size);
    neu_dvalue_free(val);

    size = neu_dvalue_deserialize(buf, size, &val1);
    EXPECT_LT(0, size);
    uint16_t ui16;
    EXPECT_EQ(0, neu_dvalue_get_uint16(val1, &ui16));
    EXPECT_EQ(16, ui16);

    free(buf);
    neu_dvalue_free(val1);

    /** uint32 serialize and deserialize test **/
    val = neu_dvalue_new(NEU_DTYPE_UINT32);
    EXPECT_EQ(0, neu_dvalue_set_uint32(val, 32));
    size = neu_dvalue_serialize(val, &buf);
    EXPECT_LT(0, size);
    neu_dvalue_free(val);

    size = neu_dvalue_deserialize(buf, size, &val1);
    EXPECT_LT(0, size);
    uint32_t ui32;
    EXPECT_EQ(0, neu_dvalue_get_uint32(val1, &ui32));
    EXPECT_EQ(32, ui32);

    free(buf);
    neu_dvalue_free(val1);

    /** uint64 serialize and deserialize test **/
    val = neu_dvalue_new(NEU_DTYPE_UINT64);
    EXPECT_EQ(0, neu_dvalue_set_uint64(val, 100));
    size = neu_dvalue_serialize(val, &buf);
    EXPECT_LT(0, size);
    neu_dvalue_free(val);

    size = neu_dvalue_deserialize(buf, size, &val1);
    EXPECT_LT(0, size);
    uint64_t ui64;
    EXPECT_EQ(0, neu_dvalue_get_uint64(val1, &ui64));
    EXPECT_EQ(100, ui64);

    free(buf);
    neu_dvalue_free(val1);

    /** float serialize and deserialize test **/
    val = neu_dvalue_new(NEU_DTYPE_FLOAT);
    EXPECT_EQ(0, neu_dvalue_set_float(val, (float) 3.14159));
    size = neu_dvalue_serialize(val, &buf);
    EXPECT_LT(0, size);
    neu_dvalue_free(val);

    size = neu_dvalue_deserialize(buf, size, &val1);
    EXPECT_LT(0, size);
    float f32;
    EXPECT_EQ(0, neu_dvalue_get_float(val1, &f32));
    EXPECT_EQ((float) 3.14159, f32);

    free(buf);
    neu_dvalue_free(val1);

    /** double serialize and deserialize **/
    val = neu_dvalue_new(NEU_DTYPE_DOUBLE);
    EXPECT_EQ(0, neu_dvalue_set_double(val, 3.14159265));
    size = neu_dvalue_serialize(val, &buf);
    EXPECT_LT(0, size);
    neu_dvalue_free(val);

    size = neu_dvalue_deserialize(buf, size, &val1);
    EXPECT_LT(0, size);
    double f64;
    EXPECT_EQ(0, neu_dvalue_get_double(val1, &f64));
    EXPECT_EQ(3.14159265, f64);

    free(buf);
    neu_dvalue_free(val1);
}

TEST(DataValueTest, neu_dvalue_cstr_deser)
{
    uint8_t *buf = NULL;
    ssize_t  size;

    neu_data_val_t *val = neu_dvalue_new(NEU_DTYPE_CSTR);
    neu_data_val_t *val_des;
    char *          get_cstr = NULL;

    EXPECT_EQ(0, neu_dvalue_set_cstr(val, (char *) "Hello, Neuron!"));
    size = neu_dvalue_serialize(val, &buf);
    EXPECT_LT(0, size);
    neu_dvalue_free(val);

    size = neu_dvalue_deserialize(buf, size, &val_des);
    EXPECT_LT(0, size);
    EXPECT_EQ(0, neu_dvalue_get_cstr(val_des, &get_cstr));
    EXPECT_STREQ("Hello, Neuron!", get_cstr);

    free(buf);
    free(get_cstr);
    neu_dvalue_free(val_des);
}

TEST(DataValueTest, neu_dvalue_keyvalue_deser)
{
    ssize_t  size;
    uint8_t *buf = NULL;

    neu_data_val_t *val;
    neu_data_val_t *val1;

    /** test int_val serialize **/
    neu_int_val_t   int_val;
    neu_data_val_t *input_val;
    input_val = neu_dvalue_new(NEU_DTYPE_CSTR);
    neu_dvalue_set_cstr(input_val, (char *) INT_VAL_TEST_STR);
    neu_int_val_init(&int_val, 21, input_val);

    val = neu_dvalue_new(NEU_DTYPE_INT_VAL);
    EXPECT_EQ(0, neu_dvalue_set_int_val(val, int_val));
    size = neu_dvalue_serialize(val, &buf);
    EXPECT_LT(0, size);
    neu_dvalue_free(val);

    size = neu_dvalue_deserialize(buf, size, &val1);
    EXPECT_LT(0, size);
    neu_int_val_t int_val_get;
    char *        cstr;
    EXPECT_EQ(0, neu_dvalue_get_int_val(val1, &int_val_get));
    EXPECT_EQ(0, neu_dvalue_get_ref_cstr(int_val_get.val, &cstr));
    EXPECT_STREQ((char *) INT_VAL_TEST_STR, cstr);
    neu_int_val_uninit(&int_val_get);

    free(buf);
    neu_dvalue_free(val1);

    /** test string_val serialize **/
    neu_string_val_t string_val;
    neu_string_t *   input_key =
        neu_string_from_cstr((char *) STRING_KEY_TEST_STR);
    neu_string_t *test_string =
        neu_string_from_cstr((char *) STRING_VAL_TEST_STR);
    input_val = neu_dvalue_new(NEU_DTYPE_STRING);
    neu_dvalue_set_move_string(input_val, test_string);
    neu_string_val_init(&string_val, input_key, input_val);

    val = neu_dvalue_new(NEU_DTYPE_STRING_VAL);
    EXPECT_EQ(0, neu_dvalue_set_string_val(val, string_val));
    size = neu_dvalue_serialize(val, &buf);
    EXPECT_LT(0, size);
    neu_dvalue_free(val);

    neu_data_val_t *val_des = NULL;
    size                    = neu_dvalue_deserialize(buf, size, &val_des);
    neu_string_val_t string_val_get;
    neu_dvalue_get_string_val(val_des, &string_val_get);
    EXPECT_STREQ((char *) STRING_KEY_TEST_STR,
                 neu_string_get_ref_cstr(string_val_get.key));
    neu_string_t *string_get = NULL;
    EXPECT_EQ(0, neu_dvalue_get_ref_string(string_val_get.val, &string_get));
    EXPECT_STREQ((char *) STRING_VAL_TEST_STR,
                 neu_string_get_ref_cstr(string_get));
    neu_string_val_uninit(&string_val_get);

    free(buf);
    neu_dvalue_free(val_des);
}

TEST(DataValueTest, neu_dvalue_array_deser)
{
    ssize_t  size;
    uint8_t *buf;
    int8_t * i8_set_buf;
    int8_t * i8_get_buf;

    int8_t             i8_arr[4] = { 2, 7, 1, 8 };
    neu_fixed_array_t *array_set = neu_fixed_array_new(4, sizeof(int8_t));
    neu_fixed_array_set(array_set, 0, &i8_arr[0]);
    neu_fixed_array_set(array_set, 1, &i8_arr[1]);
    neu_fixed_array_set(array_set, 2, &i8_arr[2]);
    neu_fixed_array_set(array_set, 3, &i8_arr[3]);

    neu_data_val_t *val = neu_dvalue_unit_new();
    neu_dvalue_init_ref_array(val, NEU_DTYPE_INT8, array_set);
    size = neu_dvalue_serialize(val, &buf);
    EXPECT_LT(0, size);
    neu_dvalue_free(val);

    neu_data_val_t *val1;
    EXPECT_LT(0, neu_dvalue_deserialize(buf, size, &val1));
    neu_fixed_array_t *array_get;
    EXPECT_EQ(0, neu_dvalue_get_move_array(val1, &array_get));
    // i8_get_buf = (int8_t *) array_get->buf;
    EXPECT_EQ(0, memcmp(array_set->buf, array_get->buf, array_get->length));
    EXPECT_EQ(array_set->esize, array_get->esize);
    EXPECT_EQ(array_set->length, array_get->length);

    free(buf);
    neu_fixed_array_free(array_set);
    neu_fixed_array_free(array_get);
    neu_dvalue_free(val1);
}

TEST(DataValueTest, neu_dvalue_2D_array_deser)
{
    uint8_t *buf = NULL;

    int64_t i64_arr[2][3] = { { 3, 2, 1 }, { 6, 5, 4 } };

    neu_fixed_array_t *array_set_2d =
        neu_fixed_array_new(2, sizeof(neu_fixed_array_t));
    neu_fixed_array_t *array_set_1 = neu_fixed_array_new(3, sizeof(int64_t));
    neu_fixed_array_t *array_set_2 = neu_fixed_array_new(3, sizeof(int64_t));

    neu_fixed_array_set(array_set_1, 0, &i64_arr[0][0]);
    neu_fixed_array_set(array_set_1, 1, &i64_arr[0][1]);
    neu_fixed_array_set(array_set_1, 2, &i64_arr[0][2]);
    neu_fixed_array_set(array_set_2, 0, &i64_arr[1][0]);
    neu_fixed_array_set(array_set_2, 1, &i64_arr[1][1]);
    neu_fixed_array_set(array_set_2, 2, &i64_arr[1][2]);

    neu_fixed_array_set(array_set_2d, 0, array_set_1);
    neu_fixed_array_set(array_set_2d, 1, array_set_2);

    neu_data_val_t *val = neu_dvalue_unit_new();
    neu_dvalue_init_ref_array(val, NEU_DTYPE_INT64, array_set_2d);
    ssize_t size = neu_dvalue_serialize(val, &buf);
    EXPECT_LT(0, size);
    neu_dvalue_free(val);

    neu_data_val_t *val_des = NULL;
    EXPECT_LT(0, neu_dvalue_deserialize(buf, size, &val_des));
    neu_fixed_array_t *array_get = NULL;
    EXPECT_EQ(0, neu_dvalue_get_move_array(val_des, &array_get));

    EXPECT_EQ(array_set_2d->esize, array_get->esize);
    EXPECT_EQ(array_set_2d->length, array_get->length);
    EXPECT_EQ(0, memcmp(array_set_2d->buf, array_get->buf, array_get->length));

    neu_fixed_array_t *array_get_1 =
        (neu_fixed_array_t *) neu_fixed_array_get(array_get, 0);
    // Two values are not equal
    EXPECT_EQ(array_set_1->esize, array_get_1->esize);
    EXPECT_EQ(array_set_1->length, array_get_1->length);
    EXPECT_EQ(0,
              memcmp(array_set_1->buf, array_get_1->buf, array_get_1->length));

    neu_fixed_array_t *array_get_2 =
        (neu_fixed_array_t *) neu_fixed_array_get(array_get, 1);
    // Two values are not equal
    EXPECT_EQ(array_set_2->esize, array_get_2->esize);
    EXPECT_EQ(array_set_2->length, array_get_2->length);
    // heap-buffer-overflow
    EXPECT_EQ(0,
              memcmp(array_set_2->buf, array_get_2->buf, array_get_2->length));

    free(buf);
    neu_fixed_array_free(array_set_1);
    neu_fixed_array_free(array_set_2);
    neu_fixed_array_free(array_set_2d);
    neu_fixed_array_free(array_get);
    neu_dvalue_free(val_des);
}

TEST(DataValuTest, neu_dvalue_3D_array_deser) { }

TEST(DataValueTest, neu_dvalue_vec_deser)
{
    int      rc;
    ssize_t  size;
    uint8_t *buf;

    vector_t *vec_set;
    int8_t    i8_arr[4] = { 2, 7, 1, 8 };
    vec_set             = vector_new(4, sizeof(int8_t));
    vector_push_back(vec_set, &i8_arr[0]);
    vector_push_back(vec_set, &i8_arr[1]);
    vector_push_back(vec_set, &i8_arr[2]);
    vector_push_back(vec_set, &i8_arr[3]);

    neu_data_val_t *val = neu_dvalue_unit_new();
    neu_dvalue_init_move_vec(val, NEU_DTYPE_INT8, vec_set);
    size = neu_dvalue_serialize(val, &buf);
    EXPECT_LT(0, size);
    neu_dvalue_free(val);

    neu_data_val_t *val1;
    size = neu_dvalue_deserialize(buf, size, &val1);
    EXPECT_LT(0, size);
    vector_t *vec_get;
    rc = neu_dvalue_get_move_vec(val1, &vec_get);
    EXPECT_EQ(0, rc);
    // TODO: assert two vector is euqal

    free(buf);
    vector_free(vec_get);
    neu_dvalue_free(val1);
}
