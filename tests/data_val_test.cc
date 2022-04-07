#include <gtest/gtest.h>

#include "data_expr.h"
#include "types.h"

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
    neu_data_val_t *input_val = neu_dvalue_new(NEU_DTYPE_CSTR);
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

neu_fixed_array_t *ser_deser_with_fixed_array(neu_fixed_array_t *array_set,
                                              neu_dtype_e        type)
{
    uint8_t *buf = NULL;
    ssize_t  size;

    neu_data_val_t *val = neu_dvalue_unit_new();
    neu_dvalue_init_ref_array(val, type, array_set);
    size = neu_dvalue_serialize(val, &buf);
    EXPECT_LT(0, size);
    neu_dvalue_free(val);

    neu_data_val_t *val_des;
    EXPECT_LT(0, neu_dvalue_deserialize(buf, size, &val_des));
    neu_fixed_array_t *array_get;
    EXPECT_EQ(0, neu_dvalue_get_move_array(val_des, &array_get));

    EXPECT_EQ(array_set->esize, array_get->esize);
    EXPECT_EQ(array_set->length, array_get->length);

    free(buf);
    neu_fixed_array_free(array_set);
    neu_dvalue_free(val_des);

    return array_get;
}

TEST(DataValueTest, neu_dvalue_array_deser)
{
    /** int64 **/
    int64_t            i64_arr[4]    = { 2, 7, 1, 8 };
    neu_fixed_array_t *set_array_int = neu_fixed_array_new(4, sizeof(int64_t));
    neu_fixed_array_set(set_array_int, 0, &i64_arr[0]);
    neu_fixed_array_set(set_array_int, 1, &i64_arr[1]);
    neu_fixed_array_set(set_array_int, 2, &i64_arr[2]);
    neu_fixed_array_set(set_array_int, 3, &i64_arr[3]);

    neu_fixed_array_t *get_array_int =
        ser_deser_with_fixed_array(set_array_int, NEU_DTYPE_INT64);
    int64_t *int_get1 = (int64_t *) neu_fixed_array_get(get_array_int, 0);
    EXPECT_EQ(2, *int_get1);
    int64_t *int_get2 = (int64_t *) neu_fixed_array_get(get_array_int, 1);
    EXPECT_EQ(7, *int_get2);
    int64_t *int_get3 = (int64_t *) neu_fixed_array_get(get_array_int, 2);
    EXPECT_EQ(1, *int_get3);
    int64_t *int_get4 = (int64_t *) neu_fixed_array_get(get_array_int, 3);
    EXPECT_EQ(8, *int_get4);
    neu_fixed_array_free(get_array_int);

    /** double **/
    double             double_arr[4] = { 0.11, 0.222, 0.3333, 0.4444 };
    neu_fixed_array_t *set_array_double =
        neu_fixed_array_new(4, sizeof(double));
    neu_fixed_array_set(set_array_double, 0, &double_arr[0]);
    neu_fixed_array_set(set_array_double, 1, &double_arr[1]);
    neu_fixed_array_set(set_array_double, 2, &double_arr[2]);
    neu_fixed_array_set(set_array_double, 3, &double_arr[3]);

    /** serialize and deserialize return **/
    neu_fixed_array_t *get_array_double =
        ser_deser_with_fixed_array(set_array_double, NEU_DTYPE_DOUBLE);

    /** Compare **/
    double *double_get1 = (double *) neu_fixed_array_get(get_array_double, 0);
    EXPECT_EQ(0.11, *double_get1);
    double *double_get2 = (double *) neu_fixed_array_get(get_array_double, 1);
    EXPECT_EQ(0.222, *double_get2);
    double *double_get3 = (double *) neu_fixed_array_get(get_array_double, 2);
    EXPECT_EQ(0.3333, *double_get3);
    double *double_get4 = (double *) neu_fixed_array_get(get_array_double, 3);
    EXPECT_EQ(0.4444, *double_get4);
    neu_fixed_array_free(get_array_double);

    /** cstr **/
    char *cstr_arr1 = (char *) "Hello,cstr-test1";
    char *cstr_arr2 = (char *) "Hello,cstr-test2";
    char *cstr_arr3 = (char *) "Hello,cstr-test3";
    char *cstr_arr4 = (char *) "Hello,cstr-test4";

    neu_fixed_array_t *set_array_cstr = neu_fixed_array_new(4, sizeof(char *));
    neu_fixed_array_set(set_array_cstr, 0, &cstr_arr1);
    neu_fixed_array_set(set_array_cstr, 1, &cstr_arr2);
    neu_fixed_array_set(set_array_cstr, 2, &cstr_arr3);
    neu_fixed_array_set(set_array_cstr, 3, &cstr_arr4);

    /** serialize and deserialize return **/
    neu_fixed_array_t *get_array_cstr =
        ser_deser_with_fixed_array(set_array_cstr, NEU_DTYPE_CSTR);

    /** Compare **/
    char *cstr_get1 = *(char **) neu_fixed_array_get(get_array_cstr, 0);
    EXPECT_STREQ((char *) "Hello,cstr-test1", cstr_get1);
    char *cstr_get2 = *(char **) neu_fixed_array_get(get_array_cstr, 1);
    EXPECT_STREQ((char *) "Hello,cstr-test2", cstr_get2);
    char *cstr_get3 = *(char **) neu_fixed_array_get(get_array_cstr, 2);
    EXPECT_STREQ((char *) "Hello,cstr-test3", cstr_get3);
    char *cstr_get4 = *(char **) neu_fixed_array_get(get_array_cstr, 3);
    EXPECT_STREQ((char *) "Hello,cstr-test4", cstr_get4);
    free(cstr_get1);
    free(cstr_get2);
    free(cstr_get3);
    free(cstr_get4);
    neu_fixed_array_free(get_array_cstr);

    /** int_val **/
    /** the fist element **/
    neu_int_val_t   int_val1;
    neu_data_val_t *input_val1 = neu_dvalue_new(NEU_DTYPE_CSTR);
    neu_dvalue_set_cstr(input_val1, (char *) "hello,int-val1");
    neu_int_val_init(&int_val1, 21, input_val1);

    /** the second element **/
    neu_int_val_t   int_val2;
    neu_data_val_t *input_val2;
    input_val2 = neu_dvalue_new(NEU_DTYPE_CSTR);
    neu_dvalue_set_cstr(input_val2, (char *) "hello,int-val2");
    neu_int_val_init(&int_val2, 22, input_val2);

    /** set array **/
    neu_fixed_array_t *set_array_int_val =
        neu_fixed_array_new(2, sizeof(neu_int_val_t));
    neu_fixed_array_set(set_array_int_val, 0, &int_val1);
    neu_fixed_array_set(set_array_int_val, 1, &int_val2);

    /** serialize and deserialize return **/
    neu_fixed_array_t *get_array_int_val =
        ser_deser_with_fixed_array(set_array_int_val, NEU_DTYPE_INT_VAL);

    /** Compare the first int_val element **/
    neu_int_val_t *get_int_val1 =
        (neu_int_val_t *) neu_fixed_array_get(get_array_int_val, 0);
    EXPECT_EQ(21, get_int_val1->key);
    char *cstr1 = NULL;
    EXPECT_EQ(0, neu_dvalue_get_ref_cstr(get_int_val1->val, &cstr1));
    EXPECT_STREQ((char *) "hello,int-val1", cstr1);
    neu_int_val_uninit(get_int_val1);

    /** Compare the second int_val element **/
    neu_int_val_t *get_int_val2 =
        (neu_int_val_t *) neu_fixed_array_get(get_array_int_val, 1);
    EXPECT_EQ(22, get_int_val2->key);
    char *cstr2 = NULL;
    EXPECT_EQ(0, neu_dvalue_get_ref_cstr(get_int_val2->val, &cstr2));
    EXPECT_STREQ((char *) "hello,int-val2", cstr2);
    neu_int_val_uninit(get_int_val2);

    neu_fixed_array_free(get_array_int_val);

    neu_dvalue_free(input_val1);
    neu_dvalue_free(input_val2);

    /** string_val **/
    /** the fist element **/
    neu_string_t *input_str_key1 = neu_string_from_cstr((char *) "string-key1");
    neu_string_t *test_string1 =
        neu_string_from_cstr((char *) "hello,string-val1");
    neu_data_val_t *input_str_val1 = neu_dvalue_new(NEU_DTYPE_STRING);
    neu_dvalue_set_move_string(input_str_val1, test_string1);
    neu_string_val_t string_val1;
    neu_string_val_init(&string_val1, input_str_key1, input_str_val1);

    /** the second element **/
    neu_string_t *input_str_key2 = neu_string_from_cstr((char *) "string-key2");
    neu_string_t *test_string2 =
        neu_string_from_cstr((char *) "hello,string-val2");
    neu_data_val_t *input_str_val2 = neu_dvalue_new(NEU_DTYPE_STRING);
    neu_dvalue_set_move_string(input_str_val2, test_string2);
    neu_string_val_t string_val2;
    neu_string_val_init(&string_val2, input_str_key2, input_str_val2);

    /** set array **/
    neu_fixed_array_t *set_array_str_val =
        neu_fixed_array_new(2, sizeof(neu_string_val_t));
    neu_fixed_array_set(set_array_str_val, 0, &string_val1);
    neu_fixed_array_set(set_array_str_val, 1, &string_val2);

    /** serialize and deserialize return **/
    neu_fixed_array_t *get_array_str_val =
        ser_deser_with_fixed_array(set_array_str_val, NEU_DTYPE_STRING_VAL);

    /** Compare the first str_val element **/
    neu_string_val_t *get_str_val1 =
        (neu_string_val_t *) neu_fixed_array_get(get_array_str_val, 0);

    EXPECT_STREQ((char *) "string-key1",
                 neu_string_get_ref_cstr(get_str_val1->key));
    neu_string_t *string_get1 = NULL;
    EXPECT_EQ(0, neu_dvalue_get_ref_string(get_str_val1->val, &string_get1));
    EXPECT_STREQ((char *) "hello,string-val1",
                 neu_string_get_ref_cstr(string_get1));
    neu_string_val_uninit(get_str_val1);

    /** Compare the second str_val element **/
    neu_string_val_t *get_str_val2 =
        (neu_string_val_t *) neu_fixed_array_get(get_array_str_val, 1);

    EXPECT_STREQ((char *) "string-key2",
                 neu_string_get_ref_cstr(get_str_val2->key));
    neu_string_t *string_get2 = NULL;
    EXPECT_EQ(0, neu_dvalue_get_ref_string(get_str_val2->val, &string_get2));
    EXPECT_STREQ((char *) "hello,string-val2",
                 neu_string_get_ref_cstr(string_get2));
    neu_string_val_uninit(get_str_val2);

    neu_fixed_array_free(get_array_str_val);

    neu_dvalue_free(input_str_val1);
    neu_dvalue_free(input_str_val2);

    neu_string_free(input_str_key1);
    neu_string_free(input_str_key2);
}

void comp_arr_2d_int(neu_fixed_array_t *get_array_int)
{
    /** Compare the first element of 2D array **/
    neu_data_val_t *get_array_dval1 =
        *(neu_data_val_t **) neu_fixed_array_get(get_array_int, 0);
    neu_fixed_array_t *get_array1 = NULL;
    neu_dvalue_get_move_array(get_array_dval1, &get_array1);
    int64_t *get_int11 = (int64_t *) neu_fixed_array_get(get_array1, 0);
    EXPECT_EQ(3, *get_int11);
    int64_t *get_int12 = (int64_t *) neu_fixed_array_get(get_array1, 1);
    EXPECT_EQ(2, *get_int12);
    int64_t *get_int13 = (int64_t *) neu_fixed_array_get(get_array1, 2);
    EXPECT_EQ(1, *get_int13);
    neu_fixed_array_free(get_array1);
    neu_dvalue_free(get_array_dval1);

    /** Compare the second element of 2D array **/
    neu_data_val_t *get_array_dval2 =
        *(neu_data_val_t **) neu_fixed_array_get(get_array_int, 1);
    neu_fixed_array_t *get_array2 = NULL;
    neu_dvalue_get_move_array(get_array_dval2, &get_array2);
    int64_t *get_int21 = (int64_t *) neu_fixed_array_get(get_array2, 0);
    EXPECT_EQ(6, *get_int21);
    int64_t *get_int22 = (int64_t *) neu_fixed_array_get(get_array2, 1);
    EXPECT_EQ(5, *get_int22);
    int64_t *get_int23 = (int64_t *) neu_fixed_array_get(get_array2, 2);
    EXPECT_EQ(4, *get_int23);
    neu_fixed_array_free(get_array2);
    neu_dvalue_free(get_array_dval2);

    neu_fixed_array_free(get_array_int);
}

void comp_arr_2d_double(neu_fixed_array_t *get_array_double)
{
    /** Compare the first element **/
    neu_data_val_t *get_array_dval1 =
        *(neu_data_val_t **) neu_fixed_array_get(get_array_double, 0);
    neu_fixed_array_t *get_array1 = NULL;
    neu_dvalue_get_move_array(get_array_dval1, &get_array1);
    double *get_double11 = (double *) neu_fixed_array_get(get_array1, 0);
    EXPECT_EQ(0.3, *get_double11);
    double *get_double12 = (double *) neu_fixed_array_get(get_array1, 1);
    EXPECT_EQ(0.2, *get_double12);
    double *get_double13 = (double *) neu_fixed_array_get(get_array1, 2);
    EXPECT_EQ(0.1, *get_double13);
    neu_fixed_array_free(get_array1);
    neu_dvalue_free(get_array_dval1);

    /** Compare the second element **/
    neu_data_val_t *get_array_dval2 =
        *(neu_data_val_t **) neu_fixed_array_get(get_array_double, 1);
    neu_fixed_array_t *get_array2 = NULL;
    neu_dvalue_get_move_array(get_array_dval2, &get_array2);
    double *get_double21 = (double *) neu_fixed_array_get(get_array2, 0);
    EXPECT_EQ(0.66, *get_double21);
    double *get_double22 = (double *) neu_fixed_array_get(get_array2, 1);
    EXPECT_EQ(0.555, *get_double22);
    double *get_double23 = (double *) neu_fixed_array_get(get_array2, 2);
    EXPECT_EQ(0.4444, *get_double23);
    neu_fixed_array_free(get_array2);
    neu_dvalue_free(get_array_dval2);

    neu_fixed_array_free(get_array_double);
}

void comp_arr_2d_int_val(neu_fixed_array_t *get_array_int_val)
{
    /** Compare the first element of 2D array **/
    neu_data_val_t *get_array_dval1 =
        *(neu_data_val_t **) neu_fixed_array_get(get_array_int_val, 0);
    neu_fixed_array_t *get_array1 = NULL;
    neu_dvalue_get_move_array(get_array_dval1, &get_array1);

    /** Compare the first element of 1D array1 **/
    neu_int_val_t *get_int_val11 =
        (neu_int_val_t *) neu_fixed_array_get(get_array1, 0);
    EXPECT_EQ(21, get_int_val11->key);
    char *output_val11 = NULL;
    EXPECT_EQ(0, neu_dvalue_get_ref_cstr(get_int_val11->val, &output_val11));
    EXPECT_STREQ((char *) "hello,int-val1", output_val11);
    neu_int_val_uninit(get_int_val11);

    /** Compare the second element of 1D array1 **/
    neu_int_val_t *get_int_val12 =
        (neu_int_val_t *) neu_fixed_array_get(get_array1, 1);
    EXPECT_EQ(22, get_int_val12->key);
    char *output_val12 = NULL;
    EXPECT_EQ(0, neu_dvalue_get_ref_cstr(get_int_val12->val, &output_val12));
    EXPECT_STREQ((char *) "hello,int-val2", output_val12);
    neu_int_val_uninit(get_int_val12);

    neu_fixed_array_free(get_array1);
    neu_dvalue_free(get_array_dval1);

    /** Compare the second element of 2D array **/
    neu_data_val_t *get_array_dval2 =
        *(neu_data_val_t **) neu_fixed_array_get(get_array_int_val, 1);
    neu_fixed_array_t *get_array2 = NULL;
    neu_dvalue_get_move_array(get_array_dval2, &get_array2);

    /** Compare the first element of 1D array2 **/
    neu_int_val_t *get_int_val21 =
        (neu_int_val_t *) neu_fixed_array_get(get_array2, 0);
    EXPECT_EQ(23, get_int_val21->key);
    char *output_val21 = NULL;
    EXPECT_EQ(0, neu_dvalue_get_ref_cstr(get_int_val21->val, &output_val21));
    EXPECT_STREQ((char *) "hello,int-val3", output_val21);
    neu_int_val_uninit(get_int_val21);

    /** Compare the second element of 1D array2 **/
    neu_int_val_t *get_int_val22 =
        (neu_int_val_t *) neu_fixed_array_get(get_array2, 1);
    EXPECT_EQ(24, get_int_val22->key);
    char *output_val22 = NULL;
    EXPECT_EQ(0, neu_dvalue_get_ref_cstr(get_int_val22->val, &output_val22));
    EXPECT_STREQ((char *) "hello,int-val4", output_val22);
    neu_int_val_uninit(get_int_val22);

    neu_fixed_array_free(get_array2);
    neu_dvalue_free(get_array_dval2);

    neu_fixed_array_free(get_array_int_val);
}

void comp_arr_2d_str_val(neu_fixed_array_t *get_array_str_val)
{
    /** Compare the first element of 2D array **/
    neu_data_val_t *get_array_dval1 =
        *(neu_data_val_t **) neu_fixed_array_get(get_array_str_val, 0);
    neu_fixed_array_t *get_array1 = NULL;
    neu_dvalue_get_move_array(get_array_dval1, &get_array1);

    /** Compare the first str_val element of 1D array1 **/
    neu_string_val_t *get_str_val11 =
        (neu_string_val_t *) neu_fixed_array_get(get_array1, 0);

    EXPECT_STREQ((char *) "string-key1",
                 neu_string_get_ref_cstr(get_str_val11->key));
    neu_string_t *get_string11 = NULL;
    EXPECT_EQ(0, neu_dvalue_get_ref_string(get_str_val11->val, &get_string11));
    EXPECT_STREQ((char *) "hello,string-val1",
                 neu_string_get_ref_cstr(get_string11));
    neu_string_val_uninit(get_str_val11);

    /** Compare the second str_val element of 1D array1 **/
    neu_string_val_t *get_str_val12 =
        (neu_string_val_t *) neu_fixed_array_get(get_array1, 1);

    EXPECT_STREQ((char *) "string-key2",
                 neu_string_get_ref_cstr(get_str_val12->key));
    neu_string_t *get_string12 = NULL;
    EXPECT_EQ(0, neu_dvalue_get_ref_string(get_str_val12->val, &get_string12));
    EXPECT_STREQ((char *) "hello,string-val2",
                 neu_string_get_ref_cstr(get_string12));
    neu_string_val_uninit(get_str_val12);

    neu_fixed_array_free(get_array1);
    neu_dvalue_free(get_array_dval1);

    /** Compare the second element of 2D array **/
    neu_data_val_t *get_array_dval2 =
        *(neu_data_val_t **) neu_fixed_array_get(get_array_str_val, 1);
    neu_fixed_array_t *get_array2 = NULL;
    neu_dvalue_get_move_array(get_array_dval2, &get_array2);

    /** Compare the first str_val element of 1D array2 **/
    neu_string_val_t *get_str_val21 =
        (neu_string_val_t *) neu_fixed_array_get(get_array2, 0);

    EXPECT_STREQ((char *) "string-key3",
                 neu_string_get_ref_cstr(get_str_val21->key));
    neu_string_t *get_string21 = NULL;
    EXPECT_EQ(0, neu_dvalue_get_ref_string(get_str_val21->val, &get_string21));
    EXPECT_STREQ((char *) "hello,string-val3",
                 neu_string_get_ref_cstr(get_string21));
    neu_string_val_uninit(get_str_val21);

    /** Compare the second str_val element of 1D array1 **/
    neu_string_val_t *get_str_val22 =
        (neu_string_val_t *) neu_fixed_array_get(get_array2, 1);

    EXPECT_STREQ((char *) "string-key4",
                 neu_string_get_ref_cstr(get_str_val22->key));
    neu_string_t *get_string22 = NULL;
    EXPECT_EQ(0, neu_dvalue_get_ref_string(get_str_val22->val, &get_string22));
    EXPECT_STREQ((char *) "hello,string-val4",
                 neu_string_get_ref_cstr(get_string22));
    neu_string_val_uninit(get_str_val22);

    neu_fixed_array_free(get_array2);
    neu_dvalue_free(get_array_dval2);

    neu_fixed_array_free(get_array_str_val);
}

TEST(DataValueTest, neu_dvalue_2D_array_deser)
{
    /** int64 **/
    /** set array **/
    int64_t i64_arr[2][3] = { { 3, 2, 1 }, { 6, 5, 4 } };

    neu_fixed_array_t *set_int_array1 = neu_fixed_array_new(3, sizeof(int64_t));
    neu_fixed_array_set(set_int_array1, 0, &i64_arr[0][0]);
    neu_fixed_array_set(set_int_array1, 1, &i64_arr[0][1]);
    neu_fixed_array_set(set_int_array1, 2, &i64_arr[0][2]);
    neu_data_val_t *array_int_dval1 = neu_dvalue_unit_new();
    neu_dvalue_init_ref_array(array_int_dval1, NEU_DTYPE_INT64, set_int_array1);

    neu_fixed_array_t *set_int_array2 = neu_fixed_array_new(3, sizeof(int64_t));
    neu_fixed_array_set(set_int_array2, 0, &i64_arr[1][0]);
    neu_fixed_array_set(set_int_array2, 1, &i64_arr[1][1]);
    neu_fixed_array_set(set_int_array2, 2, &i64_arr[1][2]);
    neu_data_val_t *array_int_dval2 = neu_dvalue_unit_new();
    neu_dvalue_init_ref_array(array_int_dval2, NEU_DTYPE_INT64, set_int_array2);

    neu_fixed_array_t *set_int_array =
        neu_fixed_array_new(2, sizeof(neu_data_val_t *));
    neu_fixed_array_set(set_int_array, 0, &array_int_dval1);
    neu_fixed_array_set(set_int_array, 1, &array_int_dval2);

    /** serialize and deserialize return **/
    neu_fixed_array_t *get_int_array =
        ser_deser_with_fixed_array(set_int_array, NEU_DTYPE_DATA_VAL);

    /** free **/
    neu_fixed_array_free(set_int_array1);
    neu_fixed_array_free(set_int_array2);

    neu_dvalue_free(array_int_dval1);
    neu_dvalue_free(array_int_dval2);

    /** Compare **/
    comp_arr_2d_int(get_int_array);

    /** double **/
    /** set array **/
    double double_arr[2][3] = { { 0.3, 0.2, 0.1 }, { 0.66, 0.555, 0.4444 } };

    /** set the first elem**/
    neu_fixed_array_t *set_double_array1 =
        neu_fixed_array_new(3, sizeof(double));
    neu_fixed_array_set(set_double_array1, 0, &double_arr[0][0]);
    neu_fixed_array_set(set_double_array1, 1, &double_arr[0][1]);
    neu_fixed_array_set(set_double_array1, 2, &double_arr[0][2]);
    neu_data_val_t *array_double_dval1 = neu_dvalue_unit_new();
    neu_dvalue_init_ref_array(array_double_dval1, NEU_DTYPE_DOUBLE,
                              set_double_array1);

    neu_fixed_array_t *set_double_array2 =
        neu_fixed_array_new(3, sizeof(double));
    neu_fixed_array_set(set_double_array2, 0, &double_arr[1][0]);
    neu_fixed_array_set(set_double_array2, 1, &double_arr[1][1]);
    neu_fixed_array_set(set_double_array2, 2, &double_arr[1][2]);
    neu_data_val_t *array_double_dval2 = neu_dvalue_unit_new();
    neu_dvalue_init_ref_array(array_double_dval2, NEU_DTYPE_DOUBLE,
                              set_double_array2);

    neu_fixed_array_t *set_double_array =
        neu_fixed_array_new(2, sizeof(neu_data_val_t *));
    neu_fixed_array_set(set_double_array, 0, &array_double_dval1);
    neu_fixed_array_set(set_double_array, 1, &array_double_dval2);

    /** serialize and deserialize return **/
    neu_fixed_array_t *get_double_array =
        ser_deser_with_fixed_array(set_double_array, NEU_DTYPE_DATA_VAL);

    /** free **/
    neu_fixed_array_free(set_double_array1);
    neu_fixed_array_free(set_double_array2);

    neu_dvalue_free(array_double_dval1);
    neu_dvalue_free(array_double_dval2);

    /** Compare **/
    comp_arr_2d_double(get_double_array);

    /** int_val **/
    /** the fist element of 1D array **/
    neu_data_val_t *input_int_val11 = neu_dvalue_new(NEU_DTYPE_CSTR);
    neu_dvalue_set_cstr(input_int_val11, (char *) "hello,int-val1");
    neu_int_val_t int_val11;
    neu_int_val_init(&int_val11, 21, input_int_val11);

    /** the second element of 1D array **/
    neu_data_val_t *input_int_val12 = neu_dvalue_new(NEU_DTYPE_CSTR);
    neu_dvalue_set_cstr(input_int_val12, (char *) "hello,int-val2");
    neu_int_val_t int_val12;
    neu_int_val_init(&int_val12, 22, input_int_val12);

    /** set the first element of 2D array **/
    neu_fixed_array_t *set_int_val_array1 =
        neu_fixed_array_new(2, sizeof(neu_int_val_t));
    neu_fixed_array_set(set_int_val_array1, 0, &int_val11);
    neu_fixed_array_set(set_int_val_array1, 1, &int_val12);

    neu_data_val_t *array_int_val_dval1 = neu_dvalue_unit_new();
    neu_dvalue_init_ref_array(array_int_val_dval1, NEU_DTYPE_INT_VAL,
                              set_int_val_array1);

    /** the third element of 1D array **/
    neu_data_val_t *input_int_val21 = neu_dvalue_new(NEU_DTYPE_CSTR);
    neu_dvalue_set_cstr(input_int_val21, (char *) "hello,int-val3");
    neu_int_val_t int_val21;
    neu_int_val_init(&int_val21, 23, input_int_val21);

    /** the fourth element of 1D array **/
    neu_data_val_t *input_int_val22 = neu_dvalue_new(NEU_DTYPE_CSTR);
    neu_dvalue_set_cstr(input_int_val22, (char *) "hello,int-val4");
    neu_int_val_t int_val22;
    neu_int_val_init(&int_val22, 24, input_int_val22);

    /** set the second element of 2D array **/
    neu_fixed_array_t *set_int_val_array2 =
        neu_fixed_array_new(2, sizeof(neu_int_val_t));
    neu_fixed_array_set(set_int_val_array2, 0, &int_val21);
    neu_fixed_array_set(set_int_val_array2, 1, &int_val22);

    neu_data_val_t *array_int_val_dval2 = neu_dvalue_unit_new();
    neu_dvalue_init_ref_array(array_int_val_dval2, NEU_DTYPE_INT_VAL,
                              set_int_val_array2);

    /** set int_val 2D array **/
    neu_fixed_array_t *set_arr_2d_int_val =
        neu_fixed_array_new(2, sizeof(neu_data_val_t *));
    neu_fixed_array_set(set_arr_2d_int_val, 0, &array_int_val_dval1);
    neu_fixed_array_set(set_arr_2d_int_val, 1, &array_int_val_dval2);

    /** serialize and deserialize return **/
    neu_fixed_array_t *get_array_int_val =
        ser_deser_with_fixed_array(set_arr_2d_int_val, NEU_DTYPE_DATA_VAL);

    /** free **/
    neu_dvalue_free(input_int_val11);
    neu_dvalue_free(input_int_val12);
    neu_dvalue_free(input_int_val21);
    neu_dvalue_free(input_int_val22);
    neu_fixed_array_free(set_int_val_array1);
    neu_dvalue_free(array_int_val_dval1);
    neu_fixed_array_free(set_int_val_array2);
    neu_dvalue_free(array_int_val_dval2);

    /* Compare **/
    comp_arr_2d_int_val(get_array_int_val);

    /** string_val **/
    /** set the first element of 1D array **/
    neu_string_t *input_str_key1 = neu_string_from_cstr((char *) "string-key1");
    neu_string_t *test_string1 =
        neu_string_from_cstr((char *) "hello,string-val1");
    neu_data_val_t *input_str_dval1 = neu_dvalue_new(NEU_DTYPE_STRING);
    neu_dvalue_set_move_string(input_str_dval1, test_string1);
    neu_string_val_t string_val1;
    neu_string_val_init(&string_val1, input_str_key1, input_str_dval1);

    /** set the second element of 1D array **/
    neu_string_t *input_str_key2 = neu_string_from_cstr((char *) "string-key2");
    neu_string_t *test_string2 =
        neu_string_from_cstr((char *) "hello,string-val2");
    neu_data_val_t *input_str_dval2 = neu_dvalue_new(NEU_DTYPE_STRING);
    neu_dvalue_set_move_string(input_str_dval2, test_string2);
    neu_string_val_t string_val2;
    neu_string_val_init(&string_val2, input_str_key2, input_str_dval2);

    /** set the first element of 2D array **/
    neu_fixed_array_t *set_str_val_array1 =
        neu_fixed_array_new(2, sizeof(neu_string_val_t));
    neu_fixed_array_set(set_str_val_array1, 0, &string_val1);
    neu_fixed_array_set(set_str_val_array1, 1, &string_val2);

    neu_data_val_t *arr_2d_str_val_dval1 = neu_dvalue_unit_new();
    neu_dvalue_init_ref_array(arr_2d_str_val_dval1, NEU_DTYPE_STRING_VAL,
                              set_str_val_array1);

    /** set the third element of 1D array **/
    neu_string_t *input_str_key3 = neu_string_from_cstr((char *) "string-key3");
    neu_string_t *test_string3 =
        neu_string_from_cstr((char *) "hello,string-val3");
    neu_data_val_t *input_str_dval3 = neu_dvalue_new(NEU_DTYPE_STRING);
    neu_dvalue_set_move_string(input_str_dval3, test_string3);
    neu_string_val_t string_val3;
    neu_string_val_init(&string_val3, input_str_key3, input_str_dval3);

    /** set the fourth element of 1D array **/
    neu_string_t *input_str_key4 = neu_string_from_cstr((char *) "string-key4");
    neu_string_t *test_string4 =
        neu_string_from_cstr((char *) "hello,string-val4");
    neu_data_val_t *input_str_dval4 = neu_dvalue_new(NEU_DTYPE_STRING);
    neu_dvalue_set_move_string(input_str_dval4, test_string4);
    neu_string_val_t string_val4;
    neu_string_val_init(&string_val4, input_str_key4, input_str_dval4);

    /** set the second element of 2D array **/
    neu_fixed_array_t *set_str_val_array2 =
        neu_fixed_array_new(2, sizeof(neu_string_val_t));
    neu_fixed_array_set(set_str_val_array2, 0, &string_val3);
    neu_fixed_array_set(set_str_val_array2, 1, &string_val4);

    neu_data_val_t *arr_2d_str_val_dval2 = neu_dvalue_unit_new();
    neu_dvalue_init_ref_array(arr_2d_str_val_dval2, NEU_DTYPE_STRING_VAL,
                              set_str_val_array2);

    /** set string_val 2D array **/
    neu_fixed_array_t *set_arr_2d_str_val =
        neu_fixed_array_new(2, sizeof(neu_data_val_t *));
    neu_fixed_array_set(set_arr_2d_str_val, 0, &arr_2d_str_val_dval1);
    neu_fixed_array_set(set_arr_2d_str_val, 1, &arr_2d_str_val_dval2);

    /** serialize and deserialize return **/
    neu_fixed_array_t *get_array_str_val =
        ser_deser_with_fixed_array(set_arr_2d_str_val, NEU_DTYPE_DATA_VAL);

    /** free **/
    neu_string_free(input_str_key1);
    neu_dvalue_free(input_str_dval1);

    neu_string_free(input_str_key2);
    neu_dvalue_free(input_str_dval2);

    neu_string_free(input_str_key3);
    neu_dvalue_free(input_str_dval3);

    neu_string_free(input_str_key4);
    neu_dvalue_free(input_str_dval4);

    neu_fixed_array_free(set_str_val_array1);
    neu_fixed_array_free(set_str_val_array2);
    neu_dvalue_free(arr_2d_str_val_dval1);
    neu_dvalue_free(arr_2d_str_val_dval2);

    /** Compare **/
    comp_arr_2d_str_val(get_array_str_val);

    /** cstr **/
    /** set array **/
    char *cstr1 = (char *) "Hello,cstr-test1";
    char *cstr2 = (char *) "Hello,cstr-test2";
    char *cstr3 = (char *) "Hello,cstr-test3";
    char *cstr4 = (char *) "Hello,cstr-test4";

    neu_fixed_array_t *set_cstr_array1 = neu_fixed_array_new(2, sizeof(char *));
    neu_fixed_array_set(set_cstr_array1, 0, &cstr1);
    neu_fixed_array_set(set_cstr_array1, 1, &cstr2);
    neu_data_val_t *array_cstr_dval1 = neu_dvalue_unit_new();
    neu_dvalue_init_ref_array(array_cstr_dval1, NEU_DTYPE_CSTR,
                              set_cstr_array1);

    neu_fixed_array_t *set_cstr_array2 = neu_fixed_array_new(2, sizeof(char *));
    neu_fixed_array_set(set_cstr_array2, 0, &cstr3);
    neu_fixed_array_set(set_cstr_array2, 1, &cstr4);
    neu_data_val_t *array_cstr_dval2 = neu_dvalue_unit_new();
    neu_dvalue_init_ref_array(array_cstr_dval2, NEU_DTYPE_CSTR,
                              set_cstr_array2);

    neu_fixed_array_t *set_cstr_array =
        neu_fixed_array_new(2, sizeof(neu_data_val_t *));
    neu_fixed_array_set(set_cstr_array, 0, &array_cstr_dval1);
    neu_fixed_array_set(set_cstr_array, 1, &array_cstr_dval2);

    /** serialize and deserialize return **/
    neu_fixed_array_t *get_cstr_array =
        ser_deser_with_fixed_array(set_cstr_array, NEU_DTYPE_DATA_VAL);

    /** Compare the first element of 2D array **/
    neu_data_val_t *get_array_dval1 =
        *(neu_data_val_t **) neu_fixed_array_get(get_cstr_array, 0);
    neu_fixed_array_t *get_array1 = NULL;
    neu_dvalue_get_move_array(get_array_dval1, &get_array1);
    char *get_cstr1 = *(char **) neu_fixed_array_get(get_array1, 0);
    EXPECT_STREQ((char *) "Hello,cstr-test1", get_cstr1);
    char *get_cstr2 = *(char **) neu_fixed_array_get(get_array1, 1);
    EXPECT_STREQ((char *) "Hello,cstr-test2", get_cstr2);
    neu_fixed_array_free(get_array1);
    neu_dvalue_free(get_array_dval1);
    free(get_cstr1);
    free(get_cstr2);

    /** Compare the second element of 2D array **/
    neu_data_val_t *get_array_dval2 =
        *(neu_data_val_t **) neu_fixed_array_get(get_cstr_array, 1);
    neu_fixed_array_t *get_array2 = NULL;
    neu_dvalue_get_move_array(get_array_dval2, &get_array2);
    char *get_cstr3 = *(char **) neu_fixed_array_get(get_array2, 0);
    EXPECT_STREQ((char *) "Hello,cstr-test3", get_cstr3);
    char *get_cstr4 = *(char **) neu_fixed_array_get(get_array2, 1);
    EXPECT_STREQ((char *) "Hello,cstr-test4", get_cstr4);
    neu_fixed_array_free(get_array2);
    neu_dvalue_free(get_array_dval2);
    free(get_cstr3);
    free(get_cstr4);

    neu_fixed_array_free(get_cstr_array);

    /** free **/
    neu_fixed_array_free(set_cstr_array1);
    neu_fixed_array_free(set_cstr_array2);

    neu_dvalue_free(array_cstr_dval1);
    neu_dvalue_free(array_cstr_dval2);
}

void comp_arr_3d_int(neu_fixed_array_t *get_array_int)
{
    /** Get the first element of the 3D array **/
    neu_data_val_t *get_array_dval1 =
        *(neu_data_val_t **) neu_fixed_array_get(get_array_int, 0);
    neu_fixed_array_t *get_array1 = NULL;
    neu_dvalue_get_move_array(get_array_dval1, &get_array1);

    /** Get the first element of the first 2D array **/
    neu_data_val_t *get_array_dval11 =
        *(neu_data_val_t **) neu_fixed_array_get(get_array1, 0);
    neu_fixed_array_t *get_array11 = NULL;
    neu_dvalue_get_move_array(get_array_dval11, &get_array11);
    int64_t *int_get111 = (int64_t *) neu_fixed_array_get(get_array11, 0);
    EXPECT_EQ(1, *int_get111);
    int64_t *int_get112 = (int64_t *) neu_fixed_array_get(get_array11, 1);
    EXPECT_EQ(2, *int_get112);
    int64_t *int_get113 = (int64_t *) neu_fixed_array_get(get_array11, 2);
    EXPECT_EQ(3, *int_get113);
    neu_fixed_array_free(get_array11);
    neu_dvalue_free(get_array_dval11);

    /** Get the second element of the first 2D array **/
    neu_data_val_t *get_array_dval12 =
        *(neu_data_val_t **) neu_fixed_array_get(get_array1, 1);
    neu_fixed_array_t *get_array12 = NULL;
    neu_dvalue_get_move_array(get_array_dval12, &get_array12);
    int64_t *int_get121 = (int64_t *) neu_fixed_array_get(get_array12, 0);
    EXPECT_EQ(11, *int_get121);
    int64_t *int_get122 = (int64_t *) neu_fixed_array_get(get_array12, 1);
    EXPECT_EQ(22, *int_get122);
    int64_t *int_get123 = (int64_t *) neu_fixed_array_get(get_array12, 2);
    EXPECT_EQ(33, *int_get123);
    neu_fixed_array_free(get_array12);
    neu_dvalue_free(get_array_dval12);

    neu_fixed_array_free(get_array1);
    neu_dvalue_free(get_array_dval1);

    /** Get the second element of the 3D array **/
    neu_data_val_t *get_array_dval2 =
        *(neu_data_val_t **) neu_fixed_array_get(get_array_int, 1);
    neu_fixed_array_t *get_array2 = NULL;
    neu_dvalue_get_move_array(get_array_dval2, &get_array2);

    /** Get the first element of the second 2D array **/
    neu_data_val_t *get_array_dval21 =
        *(neu_data_val_t **) neu_fixed_array_get(get_array2, 0);
    neu_fixed_array_t *get_array21 = NULL;
    neu_dvalue_get_move_array(get_array_dval21, &get_array21);
    int64_t *int_get311 = (int64_t *) neu_fixed_array_get(get_array21, 0);
    EXPECT_EQ(5, *int_get311);
    int64_t *int_get312 = (int64_t *) neu_fixed_array_get(get_array21, 1);
    EXPECT_EQ(6, *int_get312);
    int64_t *int_get313 = (int64_t *) neu_fixed_array_get(get_array21, 2);
    EXPECT_EQ(7, *int_get313);
    neu_fixed_array_free(get_array21);
    neu_dvalue_free(get_array_dval21);

    /** Get the second element of the second 2D array **/
    neu_data_val_t *get_array_dval22 =
        *(neu_data_val_t **) neu_fixed_array_get(get_array2, 1);
    neu_fixed_array_t *get_array22 = NULL;
    neu_dvalue_get_move_array(get_array_dval22, &get_array22);
    int64_t *int_get321 = (int64_t *) neu_fixed_array_get(get_array22, 0);
    EXPECT_EQ(55, *int_get321);
    int64_t *int_get322 = (int64_t *) neu_fixed_array_get(get_array22, 1);
    EXPECT_EQ(66, *int_get322);
    int64_t *int_get323 = (int64_t *) neu_fixed_array_get(get_array22, 2);
    EXPECT_EQ(77, *int_get323);
    neu_fixed_array_free(get_array22);
    neu_dvalue_free(get_array_dval22);

    neu_fixed_array_free(get_array2);
    neu_dvalue_free(get_array_dval2);

    neu_fixed_array_free(get_array_int);
}

void comp_arr_3d_double(neu_fixed_array_t *get_array_double)
{
    /** Get the first element of the 3D array **/
    neu_data_val_t *get_array_dval1 =
        *(neu_data_val_t **) neu_fixed_array_get(get_array_double, 0);
    neu_fixed_array_t *get_array1 = NULL;
    neu_dvalue_get_move_array(get_array_dval1, &get_array1);

    /** Get the first element of the first 2D array **/
    neu_data_val_t *get_array_dval11 =
        *(neu_data_val_t **) neu_fixed_array_get(get_array1, 0);
    neu_fixed_array_t *get_array11 = NULL;
    neu_dvalue_get_move_array(get_array_dval11, &get_array11);
    double *get_double111 = (double *) neu_fixed_array_get(get_array11, 0);
    EXPECT_EQ(0.11, *get_double111);
    double *get_double112 = (double *) neu_fixed_array_get(get_array11, 1);
    EXPECT_EQ(0.22, *get_double112);
    double *get_double113 = (double *) neu_fixed_array_get(get_array11, 2);
    EXPECT_EQ(0.33, *get_double113);
    neu_fixed_array_free(get_array11);
    neu_dvalue_free(get_array_dval11);

    /** Get the second element of the first 2D array **/
    neu_data_val_t *get_array_dval12 =
        *(neu_data_val_t **) neu_fixed_array_get(get_array1, 1);
    neu_fixed_array_t *get_array12 = NULL;
    neu_dvalue_get_move_array(get_array_dval12, &get_array12);
    double *get_double121 = (double *) neu_fixed_array_get(get_array12, 0);
    EXPECT_EQ(0.111, *get_double121);
    double *get_double122 = (double *) neu_fixed_array_get(get_array12, 1);
    EXPECT_EQ(0.222, *get_double122);
    double *get_double123 = (double *) neu_fixed_array_get(get_array12, 2);
    EXPECT_EQ(0.333, *get_double123);
    neu_fixed_array_free(get_array12);
    neu_dvalue_free(get_array_dval12);

    neu_fixed_array_free(get_array1);
    neu_dvalue_free(get_array_dval1);

    /** Get the second element of the 3D array **/
    neu_data_val_t *get_array_dval2 =
        *(neu_data_val_t **) neu_fixed_array_get(get_array_double, 1);
    neu_fixed_array_t *get_array2 = NULL;
    neu_dvalue_get_move_array(get_array_dval2, &get_array2);

    /** Get the first element of the second 2D array **/
    neu_data_val_t *get_array_dval21 =
        *(neu_data_val_t **) neu_fixed_array_get(get_array2, 0);
    neu_fixed_array_t *get_array21 = NULL;
    neu_dvalue_get_move_array(get_array_dval21, &get_array21);
    double *get_double311 = (double *) neu_fixed_array_get(get_array21, 0);
    EXPECT_EQ(0.55, *get_double311);
    double *get_double312 = (double *) neu_fixed_array_get(get_array21, 1);
    EXPECT_EQ(0.66, *get_double312);
    double *get_double313 = (double *) neu_fixed_array_get(get_array21, 2);
    EXPECT_EQ(0.77, *get_double313);
    neu_fixed_array_free(get_array21);
    neu_dvalue_free(get_array_dval21);

    /** Get the second element of the second 2D array **/
    neu_data_val_t *get_array_dval22 =
        *(neu_data_val_t **) neu_fixed_array_get(get_array2, 1);
    neu_fixed_array_t *get_array22 = NULL;
    neu_dvalue_get_move_array(get_array_dval22, &get_array22);
    double *get_double321 = (double *) neu_fixed_array_get(get_array22, 0);
    EXPECT_EQ(0.555, *get_double321);
    double *get_double322 = (double *) neu_fixed_array_get(get_array22, 1);
    EXPECT_EQ(0.666, *get_double322);
    double *get_double323 = (double *) neu_fixed_array_get(get_array22, 2);
    EXPECT_EQ(0.777, *get_double323);
    neu_fixed_array_free(get_array22);
    neu_dvalue_free(get_array_dval22);

    neu_fixed_array_free(get_array2);
    neu_dvalue_free(get_array_dval2);

    neu_fixed_array_free(get_array_double);
}

void comp_arr_3d_cstr(neu_fixed_array_t *get_array_cstr)
{
    /** Get the first element of the 3D array **/
    neu_data_val_t *get_array_dval1 =
        *(neu_data_val_t **) neu_fixed_array_get(get_array_cstr, 0);
    neu_fixed_array_t *get_array1 = NULL;
    neu_dvalue_get_move_array(get_array_dval1, &get_array1);

    /** Get the first element of the first 2D array **/
    neu_data_val_t *get_array_dval11 =
        *(neu_data_val_t **) neu_fixed_array_get(get_array1, 0);
    neu_fixed_array_t *get_array11 = NULL;
    neu_dvalue_get_move_array(get_array_dval11, &get_array11);
    char *get_cstr111 = *(char **) neu_fixed_array_get(get_array11, 0);
    EXPECT_STREQ((char *) "Hello,cstr-test1", get_cstr111);
    char *get_cstr112 = *(char **) neu_fixed_array_get(get_array11, 1);
    EXPECT_STREQ((char *) "Hello,cstr-test2", get_cstr112);
    neu_fixed_array_free(get_array11);
    neu_dvalue_free(get_array_dval11);
    free(get_cstr111);
    free(get_cstr112);

    /** Get the second element of the first 2D array **/
    neu_data_val_t *get_array_dval12 =
        *(neu_data_val_t **) neu_fixed_array_get(get_array1, 1);
    neu_fixed_array_t *get_array12 = NULL;
    neu_dvalue_get_move_array(get_array_dval12, &get_array12);
    char *get_cstr121 = *(char **) neu_fixed_array_get(get_array12, 0);
    EXPECT_STREQ((char *) "Hello,cstr-test3", get_cstr121);
    char *get_cstr122 = *(char **) neu_fixed_array_get(get_array12, 1);
    EXPECT_STREQ((char *) "Hello,cstr-test4", get_cstr122);
    neu_fixed_array_free(get_array12);
    neu_dvalue_free(get_array_dval12);
    free(get_cstr121);
    free(get_cstr122);

    neu_fixed_array_free(get_array1);
    neu_dvalue_free(get_array_dval1);

    /** Get the second element of the 3D array **/
    neu_data_val_t *get_array_dval2 =
        *(neu_data_val_t **) neu_fixed_array_get(get_array_cstr, 1);
    neu_fixed_array_t *get_array2 = NULL;
    neu_dvalue_get_move_array(get_array_dval2, &get_array2);

    /** Get the first element of the second 2D array **/
    neu_data_val_t *get_array_dval21 =
        *(neu_data_val_t **) neu_fixed_array_get(get_array2, 0);
    neu_fixed_array_t *get_array21 = NULL;
    neu_dvalue_get_move_array(get_array_dval21, &get_array21);
    char *get_cstr311 = *(char **) neu_fixed_array_get(get_array21, 0);
    EXPECT_STREQ((char *) "Hello,cstr-test5", get_cstr311);
    char *get_cstr312 = *(char **) neu_fixed_array_get(get_array21, 1);
    EXPECT_STREQ((char *) "Hello,cstr-test6", get_cstr312);
    neu_fixed_array_free(get_array21);
    neu_dvalue_free(get_array_dval21);
    free(get_cstr311);
    free(get_cstr312);

    /** Get the second element of the second 2D array **/
    neu_data_val_t *get_array_dval22 =
        *(neu_data_val_t **) neu_fixed_array_get(get_array2, 1);
    neu_fixed_array_t *get_array22 = NULL;
    neu_dvalue_get_move_array(get_array_dval22, &get_array22);
    char *get_cstr321 = *(char **) neu_fixed_array_get(get_array22, 0);
    EXPECT_STREQ((char *) "Hello,cstr-test7", get_cstr321);
    char *get_cstr322 = *(char **) neu_fixed_array_get(get_array22, 1);
    EXPECT_STREQ((char *) "Hello,cstr-test8", get_cstr322);
    neu_fixed_array_free(get_array22);
    neu_dvalue_free(get_array_dval22);
    free(get_cstr321);
    free(get_cstr322);

    neu_fixed_array_free(get_array2);
    neu_dvalue_free(get_array_dval2);

    neu_fixed_array_free(get_array_cstr);
}

void comp_arr_3d_int_val(neu_fixed_array_t *get_array_int_val)
{
    /** Get the first 2D array1 within 3D array **/
    neu_data_val_t *get_array_dval1 =
        *(neu_data_val_t **) neu_fixed_array_get(get_array_int_val, 0);
    neu_fixed_array_t *get_array1 = NULL;
    neu_dvalue_get_move_array(get_array_dval1, &get_array1);

    /** Get the first 1D array11 within the first 2D array1 **/
    neu_data_val_t *get_array_dval11 =
        *(neu_data_val_t **) neu_fixed_array_get(get_array1, 0);
    neu_fixed_array_t *get_array11 = NULL;
    neu_dvalue_get_move_array(get_array_dval11, &get_array11);

    /** Compare the first element of the first 1D array11 **/
    neu_int_val_t *get_int_val111 =
        (neu_int_val_t *) neu_fixed_array_get(get_array11, 0);
    EXPECT_EQ(111, get_int_val111->key);
    char *output_val111 = NULL;
    EXPECT_EQ(0, neu_dvalue_get_ref_cstr(get_int_val111->val, &output_val111));
    EXPECT_STREQ((char *) "hello,int-val1", output_val111);
    neu_int_val_uninit(get_int_val111);

    /** Compare the second element of the first 1D array11 **/
    neu_int_val_t *get_int_val112 =
        (neu_int_val_t *) neu_fixed_array_get(get_array11, 1);
    EXPECT_EQ(112, get_int_val112->key);
    char *output_val112 = NULL;
    EXPECT_EQ(0, neu_dvalue_get_ref_cstr(get_int_val112->val, &output_val112));
    EXPECT_STREQ((char *) "hello,int-val2", output_val112);
    neu_int_val_uninit(get_int_val112);

    neu_fixed_array_free(get_array11);
    neu_dvalue_free(get_array_dval11);

    /** Get the second 1D array12 within the first 2D array1 **/
    neu_data_val_t *get_array_dval12 =
        *(neu_data_val_t **) neu_fixed_array_get(get_array1, 1);
    neu_fixed_array_t *get_array12 = NULL;
    neu_dvalue_get_move_array(get_array_dval12, &get_array12);

    /** Compare the first element of the first 1D array12 **/
    neu_int_val_t *get_int_val121 =
        (neu_int_val_t *) neu_fixed_array_get(get_array12, 0);
    EXPECT_EQ(121, get_int_val121->key);
    char *output_val121 = NULL;
    EXPECT_EQ(0, neu_dvalue_get_ref_cstr(get_int_val121->val, &output_val121));
    EXPECT_STREQ((char *) "hello,int-val3", output_val121);
    neu_int_val_uninit(get_int_val121);

    /** Compare the second element of the first 1D array12 **/
    neu_int_val_t *get_int_val122 =
        (neu_int_val_t *) neu_fixed_array_get(get_array12, 1);
    EXPECT_EQ(122, get_int_val122->key);
    char *output_val122 = NULL;
    EXPECT_EQ(0, neu_dvalue_get_ref_cstr(get_int_val122->val, &output_val122));
    EXPECT_STREQ((char *) "hello,int-val4", output_val122);
    neu_int_val_uninit(get_int_val122);

    neu_fixed_array_free(get_array12);
    neu_dvalue_free(get_array_dval12);

    neu_fixed_array_free(get_array1);
    neu_dvalue_free(get_array_dval1);

    /** Get the second 2D array2 within 3D array **/
    neu_data_val_t *get_array_dval2 =
        *(neu_data_val_t **) neu_fixed_array_get(get_array_int_val, 1);
    neu_fixed_array_t *get_array2 = NULL;
    neu_dvalue_get_move_array(get_array_dval2, &get_array2);

    /** Get the first 1D array21 within the second 2D array2 **/
    neu_data_val_t *get_array_dval21 =
        *(neu_data_val_t **) neu_fixed_array_get(get_array2, 0);
    neu_fixed_array_t *get_array21 = NULL;
    neu_dvalue_get_move_array(get_array_dval21, &get_array21);

    /** Compare the first element of the first 1D array21 **/
    neu_int_val_t *get_int_val211 =
        (neu_int_val_t *) neu_fixed_array_get(get_array21, 0);
    EXPECT_EQ(211, get_int_val211->key);
    char *output_val211 = NULL;
    EXPECT_EQ(0, neu_dvalue_get_ref_cstr(get_int_val211->val, &output_val211));
    EXPECT_STREQ((char *) "hello,int-val5", output_val211);
    neu_int_val_uninit(get_int_val211);

    /** Compare the second element of the first 1D array21 **/
    neu_int_val_t *get_int_val212 =
        (neu_int_val_t *) neu_fixed_array_get(get_array21, 1);
    EXPECT_EQ(212, get_int_val212->key);
    char *output_val212 = NULL;
    EXPECT_EQ(0, neu_dvalue_get_ref_cstr(get_int_val212->val, &output_val212));
    EXPECT_STREQ((char *) "hello,int-val6", output_val212);
    neu_int_val_uninit(get_int_val212);

    neu_fixed_array_free(get_array21);
    neu_dvalue_free(get_array_dval21);

    /** Get the second 1D array22 within the second 2D array2 **/
    neu_data_val_t *get_array_dval22 =
        *(neu_data_val_t **) neu_fixed_array_get(get_array2, 1);
    neu_fixed_array_t *get_array22 = NULL;
    neu_dvalue_get_move_array(get_array_dval22, &get_array22);

    /** Compare the first element of the first 1D array22 **/
    neu_int_val_t *get_int_val221 =
        (neu_int_val_t *) neu_fixed_array_get(get_array22, 0);
    EXPECT_EQ(221, get_int_val221->key);
    char *output_val221 = NULL;
    EXPECT_EQ(0, neu_dvalue_get_ref_cstr(get_int_val221->val, &output_val221));
    EXPECT_STREQ((char *) "hello,int-val7", output_val221);
    neu_int_val_uninit(get_int_val221);

    /** Compare the second element of the first 1D array22 **/
    neu_int_val_t *get_int_val222 =
        (neu_int_val_t *) neu_fixed_array_get(get_array22, 1);
    EXPECT_EQ(222, get_int_val222->key);
    char *output_val222 = NULL;
    EXPECT_EQ(0, neu_dvalue_get_ref_cstr(get_int_val222->val, &output_val222));
    EXPECT_STREQ((char *) "hello,int-val8", output_val222);
    neu_int_val_uninit(get_int_val222);

    neu_fixed_array_free(get_array22);
    neu_dvalue_free(get_array_dval22);

    neu_fixed_array_free(get_array2);
    neu_dvalue_free(get_array_dval2);

    neu_fixed_array_free(get_array_int_val);
}

void comp_arr_3d_str_val(neu_fixed_array_t *get_array_str_val)
{
    /** Get the first 2D array1 within 3D array **/
    neu_data_val_t *get_array_dval1 =
        *(neu_data_val_t **) neu_fixed_array_get(get_array_str_val, 0);
    neu_fixed_array_t *get_array1 = NULL;
    neu_dvalue_get_move_array(get_array_dval1, &get_array1);

    /** Get the first 1D array11 within the first 2D array1 **/
    neu_data_val_t *get_array_dval11 =
        *(neu_data_val_t **) neu_fixed_array_get(get_array1, 0);
    neu_fixed_array_t *get_array11 = NULL;
    neu_dvalue_get_move_array(get_array_dval11, &get_array11);

    /** Compare the first element of the first 1D array11 **/
    neu_string_val_t *get_str_val111 =
        (neu_string_val_t *) neu_fixed_array_get(get_array11, 0);
    EXPECT_STREQ((char *) "string-key1",
                 neu_string_get_ref_cstr(get_str_val111->key));
    neu_string_t *get_string111 = NULL;
    EXPECT_EQ(0,
              neu_dvalue_get_ref_string(get_str_val111->val, &get_string111));
    EXPECT_STREQ((char *) "hello,string-val1",
                 neu_string_get_ref_cstr(get_string111));
    neu_string_val_uninit(get_str_val111);

    /** Compare the second element of the first 1D array11 **/
    neu_string_val_t *get_str_val112 =
        (neu_string_val_t *) neu_fixed_array_get(get_array11, 1);
    EXPECT_STREQ((char *) "string-key2",
                 neu_string_get_ref_cstr(get_str_val112->key));
    neu_string_t *get_string112 = NULL;
    EXPECT_EQ(0,
              neu_dvalue_get_ref_string(get_str_val112->val, &get_string112));
    EXPECT_STREQ((char *) "hello,string-val2",
                 neu_string_get_ref_cstr(get_string112));
    neu_string_val_uninit(get_str_val112);

    neu_fixed_array_free(get_array11);
    neu_dvalue_free(get_array_dval11);

    /** Get the second 1D array12 within the first 2D array1 **/
    neu_data_val_t *get_array_dval12 =
        *(neu_data_val_t **) neu_fixed_array_get(get_array1, 1);
    neu_fixed_array_t *get_array12 = NULL;
    neu_dvalue_get_move_array(get_array_dval12, &get_array12);

    /** Compare the first element of the first 1D array12 **/
    neu_string_val_t *get_str_val121 =
        (neu_string_val_t *) neu_fixed_array_get(get_array12, 0);
    EXPECT_STREQ((char *) "string-key3",
                 neu_string_get_ref_cstr(get_str_val121->key));
    neu_string_t *get_string121 = NULL;
    EXPECT_EQ(0,
              neu_dvalue_get_ref_string(get_str_val121->val, &get_string121));
    EXPECT_STREQ((char *) "hello,string-val3",
                 neu_string_get_ref_cstr(get_string121));
    neu_string_val_uninit(get_str_val121);

    /** Compare the second element of the first 1D array12 **/
    neu_string_val_t *get_str_val122 =
        (neu_string_val_t *) neu_fixed_array_get(get_array12, 1);
    EXPECT_STREQ((char *) "string-key4",
                 neu_string_get_ref_cstr(get_str_val122->key));
    neu_string_t *get_string122 = NULL;
    EXPECT_EQ(0,
              neu_dvalue_get_ref_string(get_str_val122->val, &get_string122));
    EXPECT_STREQ((char *) "hello,string-val4",
                 neu_string_get_ref_cstr(get_string122));
    neu_string_val_uninit(get_str_val122);

    neu_fixed_array_free(get_array12);
    neu_dvalue_free(get_array_dval12);

    neu_fixed_array_free(get_array1);
    neu_dvalue_free(get_array_dval1);

    /** Get the second 2D array2 within 3D array **/
    neu_data_val_t *get_array_dval2 =
        *(neu_data_val_t **) neu_fixed_array_get(get_array_str_val, 1);
    neu_fixed_array_t *get_array2 = NULL;
    neu_dvalue_get_move_array(get_array_dval2, &get_array2);

    /** Get the first 1D array21 within the second 2D array2 **/
    neu_data_val_t *get_array_dval21 =
        *(neu_data_val_t **) neu_fixed_array_get(get_array2, 0);
    neu_fixed_array_t *get_array21 = NULL;
    neu_dvalue_get_move_array(get_array_dval21, &get_array21);

    /** Compare the first element of the first 1D array21 **/
    neu_string_val_t *get_str_val211 =
        (neu_string_val_t *) neu_fixed_array_get(get_array21, 0);
    EXPECT_STREQ((char *) "string-key5",
                 neu_string_get_ref_cstr(get_str_val211->key));
    neu_string_t *get_string211 = NULL;
    EXPECT_EQ(0,
              neu_dvalue_get_ref_string(get_str_val211->val, &get_string211));
    EXPECT_STREQ((char *) "hello,string-val5",
                 neu_string_get_ref_cstr(get_string211));
    neu_string_val_uninit(get_str_val211);

    /** Compare the second element of the first 1D array21 **/
    neu_string_val_t *get_str_val212 =
        (neu_string_val_t *) neu_fixed_array_get(get_array21, 1);
    EXPECT_STREQ((char *) "string-key6",
                 neu_string_get_ref_cstr(get_str_val212->key));
    neu_string_t *get_string212 = NULL;
    EXPECT_EQ(0,
              neu_dvalue_get_ref_string(get_str_val212->val, &get_string212));
    EXPECT_STREQ((char *) "hello,string-val6",
                 neu_string_get_ref_cstr(get_string212));
    neu_string_val_uninit(get_str_val212);

    neu_fixed_array_free(get_array21);
    neu_dvalue_free(get_array_dval21);

    /** Get the second 1D array22 within the second 2D array2 **/
    neu_data_val_t *get_array_dval22 =
        *(neu_data_val_t **) neu_fixed_array_get(get_array2, 1);
    neu_fixed_array_t *get_array22 = NULL;
    neu_dvalue_get_move_array(get_array_dval22, &get_array22);

    /** Compare the first element of the first 1D array22 **/
    neu_string_val_t *get_str_val221 =
        (neu_string_val_t *) neu_fixed_array_get(get_array22, 0);
    EXPECT_STREQ((char *) "string-key7",
                 neu_string_get_ref_cstr(get_str_val221->key));
    neu_string_t *get_string221 = NULL;
    EXPECT_EQ(0,
              neu_dvalue_get_ref_string(get_str_val221->val, &get_string221));
    EXPECT_STREQ((char *) "hello,string-val7",
                 neu_string_get_ref_cstr(get_string221));
    neu_string_val_uninit(get_str_val221);

    /** Compare the second element of the first 1D array22 **/
    neu_string_val_t *get_str_val222 =
        (neu_string_val_t *) neu_fixed_array_get(get_array22, 1);
    EXPECT_STREQ((char *) "string-key8",
                 neu_string_get_ref_cstr(get_str_val222->key));
    neu_string_t *get_string222 = NULL;
    EXPECT_EQ(0,
              neu_dvalue_get_ref_string(get_str_val222->val, &get_string222));
    EXPECT_STREQ((char *) "hello,string-val8",
                 neu_string_get_ref_cstr(get_string222));
    neu_string_val_uninit(get_str_val222);

    neu_fixed_array_free(get_array22);
    neu_dvalue_free(get_array_dval22);

    neu_fixed_array_free(get_array2);
    neu_dvalue_free(get_array_dval2);

    neu_fixed_array_free(get_array_str_val);
}

TEST(DataValuTest, neu_dvalue_3D_array_deser)
{
    /** int64 **/
    int64_t i64_arr[2][2][3] = { { { 1, 2, 3 }, { 11, 22, 33 } },
                                 { { 5, 6, 7 }, { 55, 66, 77 } } };

    neu_fixed_array_t *set_array_int11 =
        neu_fixed_array_new(3, sizeof(int64_t));
    neu_fixed_array_set(set_array_int11, 0, &i64_arr[0][0][0]);
    neu_fixed_array_set(set_array_int11, 1, &i64_arr[0][0][1]);
    neu_fixed_array_set(set_array_int11, 2, &i64_arr[0][0][2]);
    neu_fixed_array_t *set_array_int12 =
        neu_fixed_array_new(3, sizeof(int64_t));
    neu_fixed_array_set(set_array_int12, 0, &i64_arr[0][1][0]);
    neu_fixed_array_set(set_array_int12, 1, &i64_arr[0][1][1]);
    neu_fixed_array_set(set_array_int12, 2, &i64_arr[0][1][2]);
    neu_fixed_array_t *set_array_int21 =
        neu_fixed_array_new(3, sizeof(int64_t));
    neu_fixed_array_set(set_array_int21, 0, &i64_arr[1][0][0]);
    neu_fixed_array_set(set_array_int21, 1, &i64_arr[1][0][1]);
    neu_fixed_array_set(set_array_int21, 2, &i64_arr[1][0][2]);
    neu_fixed_array_t *set_array_int22 =
        neu_fixed_array_new(3, sizeof(int64_t));
    neu_fixed_array_set(set_array_int22, 0, &i64_arr[1][1][0]);
    neu_fixed_array_set(set_array_int22, 1, &i64_arr[1][1][1]);
    neu_fixed_array_set(set_array_int22, 2, &i64_arr[1][1][2]);

    neu_data_val_t *array_int_dval11 = neu_dvalue_unit_new();
    neu_dvalue_init_ref_array(array_int_dval11, NEU_DTYPE_INT64,
                              set_array_int11);
    neu_data_val_t *array_int_dval12 = neu_dvalue_unit_new();
    neu_dvalue_init_ref_array(array_int_dval12, NEU_DTYPE_INT64,
                              set_array_int12);
    neu_data_val_t *array_int_dval21 = neu_dvalue_unit_new();
    neu_dvalue_init_ref_array(array_int_dval21, NEU_DTYPE_INT64,
                              set_array_int21);
    neu_data_val_t *array_int_dval22 = neu_dvalue_unit_new();
    neu_dvalue_init_ref_array(array_int_dval22, NEU_DTYPE_INT64,
                              set_array_int22);

    neu_fixed_array_t *set_array1 =
        neu_fixed_array_new(2, sizeof(neu_data_val_t *));
    neu_fixed_array_set(set_array1, 0, &array_int_dval11);
    neu_fixed_array_set(set_array1, 1, &array_int_dval12);
    neu_fixed_array_t *set_array2 =
        neu_fixed_array_new(2, sizeof(neu_data_val_t *));
    neu_fixed_array_set(set_array2, 0, &array_int_dval21);
    neu_fixed_array_set(set_array2, 1, &array_int_dval22);

    neu_data_val_t *array_int_dval1 = neu_dvalue_unit_new();
    neu_dvalue_init_ref_array(array_int_dval1, NEU_DTYPE_DATA_VAL, set_array1);
    neu_data_val_t *array_int_dval2 = neu_dvalue_unit_new();
    neu_dvalue_init_ref_array(array_int_dval2, NEU_DTYPE_DATA_VAL, set_array2);

    neu_fixed_array_t *set_array_3d =
        neu_fixed_array_new(2, sizeof(neu_data_val_t *));
    neu_fixed_array_set(set_array_3d, 0, &array_int_dval1);
    neu_fixed_array_set(set_array_3d, 1, &array_int_dval2);

    /** serialize and deserialize return **/
    neu_fixed_array_t *get_array_int =
        ser_deser_with_fixed_array(set_array_3d, NEU_DTYPE_DATA_VAL);

    /** free **/
    neu_fixed_array_free(set_array_int11);
    neu_fixed_array_free(set_array_int12);
    neu_fixed_array_free(set_array_int21);
    neu_fixed_array_free(set_array_int22);

    neu_dvalue_free(array_int_dval11);
    neu_dvalue_free(array_int_dval12);
    neu_dvalue_free(array_int_dval21);
    neu_dvalue_free(array_int_dval22);

    neu_fixed_array_free(set_array1);
    neu_fixed_array_free(set_array2);

    neu_dvalue_free(array_int_dval1);
    neu_dvalue_free(array_int_dval2);

    /** Compare **/
    comp_arr_3d_int(get_array_int);

    /** double **/
    /** set array **/
    double double_arr[2][2][3] = {
        { { 0.11, 0.22, 0.33 }, { 0.111, 0.222, 0.333 } },
        { { 0.55, 0.66, 0.77 }, { 0.555, 0.666, 0.777 } }
    };
    neu_fixed_array_t *set_array_double11 =
        neu_fixed_array_new(3, sizeof(double));
    neu_fixed_array_set(set_array_double11, 0, &double_arr[0][0][0]);
    neu_fixed_array_set(set_array_double11, 1, &double_arr[0][0][1]);
    neu_fixed_array_set(set_array_double11, 2, &double_arr[0][0][2]);
    neu_data_val_t *array_double_dval11 = neu_dvalue_unit_new();
    neu_dvalue_init_ref_array(array_double_dval11, NEU_DTYPE_DOUBLE,
                              set_array_double11);

    neu_fixed_array_t *set_array_double12 =
        neu_fixed_array_new(3, sizeof(double));
    neu_fixed_array_set(set_array_double12, 0, &double_arr[0][1][0]);
    neu_fixed_array_set(set_array_double12, 1, &double_arr[0][1][1]);
    neu_fixed_array_set(set_array_double12, 2, &double_arr[0][1][2]);
    neu_data_val_t *array_double_dval12 = neu_dvalue_unit_new();
    neu_dvalue_init_ref_array(array_double_dval12, NEU_DTYPE_DOUBLE,
                              set_array_double12);

    neu_fixed_array_t *set_array_double21 =
        neu_fixed_array_new(3, sizeof(double));
    neu_fixed_array_set(set_array_double21, 0, &double_arr[1][0][0]);
    neu_fixed_array_set(set_array_double21, 1, &double_arr[1][0][1]);
    neu_fixed_array_set(set_array_double21, 2, &double_arr[1][0][2]);
    neu_data_val_t *array_double_dval21 = neu_dvalue_unit_new();
    neu_dvalue_init_ref_array(array_double_dval21, NEU_DTYPE_DOUBLE,
                              set_array_double21);

    neu_fixed_array_t *set_array_double22 =
        neu_fixed_array_new(3, sizeof(double));
    neu_fixed_array_set(set_array_double22, 0, &double_arr[1][1][0]);
    neu_fixed_array_set(set_array_double22, 1, &double_arr[1][1][1]);
    neu_fixed_array_set(set_array_double22, 2, &double_arr[1][1][2]);
    neu_data_val_t *array_double_dval22 = neu_dvalue_unit_new();
    neu_dvalue_init_ref_array(array_double_dval22, NEU_DTYPE_DOUBLE,
                              set_array_double22);

    neu_fixed_array_t *set_array_double1 =
        neu_fixed_array_new(2, sizeof(neu_data_val_t *));
    neu_fixed_array_set(set_array_double1, 0, &array_double_dval11);
    neu_fixed_array_set(set_array_double1, 1, &array_double_dval12);
    neu_data_val_t *array_double_dval1 = neu_dvalue_unit_new();
    neu_dvalue_init_ref_array(array_double_dval1, NEU_DTYPE_DATA_VAL,
                              set_array_double1);

    neu_fixed_array_t *set_array_double2 =
        neu_fixed_array_new(2, sizeof(neu_data_val_t *));
    neu_fixed_array_set(set_array_double2, 0, &array_double_dval21);
    neu_fixed_array_set(set_array_double2, 1, &array_double_dval22);
    neu_data_val_t *array_double_dval2 = neu_dvalue_unit_new();
    neu_dvalue_init_ref_array(array_double_dval2, NEU_DTYPE_DATA_VAL,
                              set_array_double2);

    neu_fixed_array_t *set_array_double =
        neu_fixed_array_new(2, sizeof(neu_data_val_t *));
    neu_fixed_array_set(set_array_double, 0, &array_double_dval1);
    neu_fixed_array_set(set_array_double, 1, &array_double_dval2);

    /** serialize and deserialize return **/
    neu_fixed_array_t *get_array_double =
        ser_deser_with_fixed_array(set_array_double, NEU_DTYPE_DATA_VAL);

    /** free **/
    neu_fixed_array_free(set_array_double11);
    neu_fixed_array_free(set_array_double12);
    neu_fixed_array_free(set_array_double21);
    neu_fixed_array_free(set_array_double22);

    neu_dvalue_free(array_double_dval11);
    neu_dvalue_free(array_double_dval12);
    neu_dvalue_free(array_double_dval21);
    neu_dvalue_free(array_double_dval22);

    neu_fixed_array_free(set_array_double1);
    neu_fixed_array_free(set_array_double2);

    neu_dvalue_free(array_double_dval1);
    neu_dvalue_free(array_double_dval2);

    /** Compare **/
    comp_arr_3d_double(get_array_double);

    /** cstr **/
    /** set array **/
    char *cstr1 = (char *) "Hello,cstr-test1";
    char *cstr2 = (char *) "Hello,cstr-test2";
    char *cstr3 = (char *) "Hello,cstr-test3";
    char *cstr4 = (char *) "Hello,cstr-test4";
    char *cstr5 = (char *) "Hello,cstr-test5";
    char *cstr6 = (char *) "Hello,cstr-test6";
    char *cstr7 = (char *) "Hello,cstr-test7";
    char *cstr8 = (char *) "Hello,cstr-test8";

    neu_fixed_array_t *set_array_cstr11 =
        neu_fixed_array_new(2, sizeof(char *));
    neu_fixed_array_set(set_array_cstr11, 0, &cstr1);
    neu_fixed_array_set(set_array_cstr11, 1, &cstr2);
    neu_data_val_t *array_cstr_dval11 = neu_dvalue_unit_new();
    neu_dvalue_init_ref_array(array_cstr_dval11, NEU_DTYPE_CSTR,
                              set_array_cstr11);

    neu_fixed_array_t *set_array_cstr12 =
        neu_fixed_array_new(2, sizeof(char *));
    neu_fixed_array_set(set_array_cstr12, 0, &cstr3);
    neu_fixed_array_set(set_array_cstr12, 1, &cstr4);
    neu_data_val_t *array_cstr_dval12 = neu_dvalue_unit_new();
    neu_dvalue_init_ref_array(array_cstr_dval12, NEU_DTYPE_CSTR,
                              set_array_cstr12);

    neu_fixed_array_t *set_array_cstr21 =
        neu_fixed_array_new(2, sizeof(char *));
    neu_fixed_array_set(set_array_cstr21, 0, &cstr5);
    neu_fixed_array_set(set_array_cstr21, 1, &cstr6);
    neu_data_val_t *array_cstr_dval21 = neu_dvalue_unit_new();
    neu_dvalue_init_ref_array(array_cstr_dval21, NEU_DTYPE_CSTR,
                              set_array_cstr21);

    neu_fixed_array_t *set_array_cstr22 =
        neu_fixed_array_new(2, sizeof(char *));
    neu_fixed_array_set(set_array_cstr22, 0, &cstr7);
    neu_fixed_array_set(set_array_cstr22, 1, &cstr8);
    neu_data_val_t *array_cstr_dval22 = neu_dvalue_unit_new();
    neu_dvalue_init_ref_array(array_cstr_dval22, NEU_DTYPE_CSTR,
                              set_array_cstr22);

    neu_fixed_array_t *set_array_cstr1 =
        neu_fixed_array_new(2, sizeof(neu_data_val_t *));
    neu_fixed_array_set(set_array_cstr1, 0, &array_cstr_dval11);
    neu_fixed_array_set(set_array_cstr1, 1, &array_cstr_dval12);
    neu_data_val_t *array_cstr_dval1 = neu_dvalue_unit_new();
    neu_dvalue_init_ref_array(array_cstr_dval1, NEU_DTYPE_DATA_VAL,
                              set_array_cstr1);

    neu_fixed_array_t *set_array_cstr2 =
        neu_fixed_array_new(2, sizeof(neu_data_val_t *));
    neu_fixed_array_set(set_array_cstr2, 0, &array_cstr_dval21);
    neu_fixed_array_set(set_array_cstr2, 1, &array_cstr_dval22);
    neu_data_val_t *array_cstr_dval2 = neu_dvalue_unit_new();
    neu_dvalue_init_ref_array(array_cstr_dval2, NEU_DTYPE_DATA_VAL,
                              set_array_cstr2);

    neu_fixed_array_t *set_array_cstr =
        neu_fixed_array_new(2, sizeof(neu_data_val_t *));
    neu_fixed_array_set(set_array_cstr, 0, &array_cstr_dval1);
    neu_fixed_array_set(set_array_cstr, 1, &array_cstr_dval2);

    /** serialize and deserialize return **/
    neu_fixed_array_t *get_array_cstr =
        ser_deser_with_fixed_array(set_array_cstr, NEU_DTYPE_DATA_VAL);

    /** Compare **/
    comp_arr_3d_cstr(get_array_cstr);

    /** free **/
    neu_fixed_array_free(set_array_cstr11);
    neu_fixed_array_free(set_array_cstr12);
    neu_fixed_array_free(set_array_cstr21);
    neu_fixed_array_free(set_array_cstr22);

    neu_dvalue_free(array_cstr_dval11);
    neu_dvalue_free(array_cstr_dval12);
    neu_dvalue_free(array_cstr_dval21);
    neu_dvalue_free(array_cstr_dval22);

    neu_fixed_array_free(set_array_cstr1);
    neu_fixed_array_free(set_array_cstr2);

    neu_dvalue_free(array_cstr_dval1);
    neu_dvalue_free(array_cstr_dval2);

    /** int_val **/
    /** the fist element of 1D array **/
    neu_data_val_t *input_int_val111 = neu_dvalue_new(NEU_DTYPE_CSTR);
    neu_dvalue_set_cstr(input_int_val111, (char *) "hello,int-val1");
    neu_int_val_t int_val111;
    neu_int_val_init(&int_val111, 111, input_int_val111);

    /** the second element of 1D array **/
    neu_data_val_t *input_int_val112 = neu_dvalue_new(NEU_DTYPE_CSTR);
    neu_dvalue_set_cstr(input_int_val112, (char *) "hello,int-val2");
    neu_int_val_t int_val112;
    neu_int_val_init(&int_val112, 112, input_int_val112);

    /** the third element of 1D array **/
    neu_data_val_t *input_int_val121 = neu_dvalue_new(NEU_DTYPE_CSTR);
    neu_dvalue_set_cstr(input_int_val121, (char *) "hello,int-val3");
    neu_int_val_t int_val121;
    neu_int_val_init(&int_val121, 121, input_int_val121);

    /** the fourth element of 1D array **/
    neu_data_val_t *input_int_val122 = neu_dvalue_new(NEU_DTYPE_CSTR);
    neu_dvalue_set_cstr(input_int_val122, (char *) "hello,int-val4");
    neu_int_val_t int_val122;
    neu_int_val_init(&int_val122, 122, input_int_val122);

    /** the fifth element of 1D array **/
    neu_data_val_t *input_int_val211 = neu_dvalue_new(NEU_DTYPE_CSTR);
    neu_dvalue_set_cstr(input_int_val211, (char *) "hello,int-val5");
    neu_int_val_t int_val211;
    neu_int_val_init(&int_val211, 211, input_int_val211);

    /** the sixth element of 1D array **/
    neu_data_val_t *input_int_val212 = neu_dvalue_new(NEU_DTYPE_CSTR);
    neu_dvalue_set_cstr(input_int_val212, (char *) "hello,int-val6");
    neu_int_val_t int_val212;
    neu_int_val_init(&int_val212, 212, input_int_val212);

    /** the seventh element of 1D array **/
    neu_data_val_t *input_int_val221 = neu_dvalue_new(NEU_DTYPE_CSTR);
    neu_dvalue_set_cstr(input_int_val221, (char *) "hello,int-val7");
    neu_int_val_t int_val221;
    neu_int_val_init(&int_val221, 221, input_int_val221);

    /** the eighth element of 1D array **/
    neu_data_val_t *input_int_val222 = neu_dvalue_new(NEU_DTYPE_CSTR);
    neu_dvalue_set_cstr(input_int_val222, (char *) "hello,int-val8");
    neu_int_val_t int_val222;
    neu_int_val_init(&int_val222, 222, input_int_val222);

    /** set the first element of 2D array11 **/
    neu_fixed_array_t *set_array_int_val11 =
        neu_fixed_array_new(2, sizeof(neu_int_val_t));
    neu_fixed_array_set(set_array_int_val11, 0, &int_val111);
    neu_fixed_array_set(set_array_int_val11, 1, &int_val112);
    neu_data_val_t *array_int_val_dval11 = neu_dvalue_unit_new();
    neu_dvalue_init_ref_array(array_int_val_dval11, NEU_DTYPE_INT_VAL,
                              set_array_int_val11);

    /** set the second element of 2D array12 **/
    neu_fixed_array_t *set_array_int_val12 =
        neu_fixed_array_new(2, sizeof(neu_int_val_t));
    neu_fixed_array_set(set_array_int_val12, 0, &int_val121);
    neu_fixed_array_set(set_array_int_val12, 1, &int_val122);
    neu_data_val_t *array_int_val_dval12 = neu_dvalue_unit_new();
    neu_dvalue_init_ref_array(array_int_val_dval12, NEU_DTYPE_INT_VAL,
                              set_array_int_val12);

    /** set the first element of 2D array21 **/
    neu_fixed_array_t *set_array_int_val21 =
        neu_fixed_array_new(2, sizeof(neu_int_val_t));
    neu_fixed_array_set(set_array_int_val21, 0, &int_val211);
    neu_fixed_array_set(set_array_int_val21, 1, &int_val212);
    neu_data_val_t *array_int_val_dval21 = neu_dvalue_unit_new();
    neu_dvalue_init_ref_array(array_int_val_dval21, NEU_DTYPE_INT_VAL,
                              set_array_int_val21);

    /** set the second element of 2D array22 **/
    neu_fixed_array_t *set_array_int_val22 =
        neu_fixed_array_new(2, sizeof(neu_int_val_t));
    neu_fixed_array_set(set_array_int_val22, 0, &int_val221);
    neu_fixed_array_set(set_array_int_val22, 1, &int_val222);
    neu_data_val_t *array_int_val_dval22 = neu_dvalue_unit_new();
    neu_dvalue_init_ref_array(array_int_val_dval22, NEU_DTYPE_INT_VAL,
                              set_array_int_val22);

    /** set the first element of 3D array1 **/
    neu_fixed_array_t *set_array_int_val1 =
        neu_fixed_array_new(2, sizeof(neu_data_val_t *));
    neu_fixed_array_set(set_array_int_val1, 0, &array_int_val_dval11);
    neu_fixed_array_set(set_array_int_val1, 1, &array_int_val_dval12);
    neu_data_val_t *array_int_val_dval1 = neu_dvalue_unit_new();
    neu_dvalue_init_ref_array(array_int_val_dval1, NEU_DTYPE_DATA_VAL,
                              set_array_int_val1);

    /** set the second element of 3D array1 **/
    neu_fixed_array_t *set_array_int_val2 =
        neu_fixed_array_new(2, sizeof(neu_data_val_t *));
    neu_fixed_array_set(set_array_int_val2, 0, &array_int_val_dval21);
    neu_fixed_array_set(set_array_int_val2, 1, &array_int_val_dval22);
    neu_data_val_t *array_int_val_dval2 = neu_dvalue_unit_new();
    neu_dvalue_init_ref_array(array_int_val_dval2, NEU_DTYPE_DATA_VAL,
                              set_array_int_val2);

    /** set 3D array **/
    neu_fixed_array_t *set_array_int_val =
        neu_fixed_array_new(2, sizeof(neu_data_val_t *));
    neu_fixed_array_set(set_array_int_val, 0, &array_int_val_dval1);
    neu_fixed_array_set(set_array_int_val, 1, &array_int_val_dval2);

    /** serialize and deserialize return **/
    neu_fixed_array_t *get_array_int_val =
        ser_deser_with_fixed_array(set_array_int_val, NEU_DTYPE_DATA_VAL);

    /** free **/
    neu_dvalue_free(input_int_val111);
    neu_dvalue_free(input_int_val112);
    neu_dvalue_free(input_int_val121);
    neu_dvalue_free(input_int_val122);
    neu_dvalue_free(input_int_val211);
    neu_dvalue_free(input_int_val212);
    neu_dvalue_free(input_int_val221);
    neu_dvalue_free(input_int_val222);

    neu_fixed_array_free(set_array_int_val11);
    neu_fixed_array_free(set_array_int_val12);
    neu_fixed_array_free(set_array_int_val21);
    neu_fixed_array_free(set_array_int_val22);

    neu_dvalue_free(array_int_val_dval11);
    neu_dvalue_free(array_int_val_dval12);
    neu_dvalue_free(array_int_val_dval21);
    neu_dvalue_free(array_int_val_dval22);

    neu_fixed_array_free(set_array_int_val1);
    neu_fixed_array_free(set_array_int_val2);

    neu_dvalue_free(array_int_val_dval1);
    neu_dvalue_free(array_int_val_dval2);

    /** Compare **/
    comp_arr_3d_int_val(get_array_int_val);

    /** string_val **/
    /** set the first element of 1D array **/
    neu_string_t *input_str_key1 = neu_string_from_cstr((char *) "string-key1");
    neu_string_t *test_string1 =
        neu_string_from_cstr((char *) "hello,string-val1");
    neu_data_val_t *input_str_dval1 = neu_dvalue_new(NEU_DTYPE_STRING);
    neu_dvalue_set_move_string(input_str_dval1, test_string1);
    neu_string_val_t string_val1;
    neu_string_val_init(&string_val1, input_str_key1, input_str_dval1);

    /** set the second element of 1D array **/
    neu_string_t *input_str_key2 = neu_string_from_cstr((char *) "string-key2");
    neu_string_t *test_string2 =
        neu_string_from_cstr((char *) "hello,string-val2");
    neu_data_val_t *input_str_dval2 = neu_dvalue_new(NEU_DTYPE_STRING);
    neu_dvalue_set_move_string(input_str_dval2, test_string2);
    neu_string_val_t string_val2;
    neu_string_val_init(&string_val2, input_str_key2, input_str_dval2);

    /** set the third element of 1D array **/
    neu_string_t *input_str_key3 = neu_string_from_cstr((char *) "string-key3");
    neu_string_t *test_string3 =
        neu_string_from_cstr((char *) "hello,string-val3");
    neu_data_val_t *input_str_dval3 = neu_dvalue_new(NEU_DTYPE_STRING);
    neu_dvalue_set_move_string(input_str_dval3, test_string3);
    neu_string_val_t string_val3;
    neu_string_val_init(&string_val3, input_str_key3, input_str_dval3);

    /** set the fourth element of 1D array **/
    neu_string_t *input_str_key4 = neu_string_from_cstr((char *) "string-key4");
    neu_string_t *test_string4 =
        neu_string_from_cstr((char *) "hello,string-val4");
    neu_data_val_t *input_str_dval4 = neu_dvalue_new(NEU_DTYPE_STRING);
    neu_dvalue_set_move_string(input_str_dval4, test_string4);
    neu_string_val_t string_val4;
    neu_string_val_init(&string_val4, input_str_key4, input_str_dval4);

    /** set the fifth element of 1D array **/
    neu_string_t *input_str_key5 = neu_string_from_cstr((char *) "string-key5");
    neu_string_t *test_string5 =
        neu_string_from_cstr((char *) "hello,string-val5");
    neu_data_val_t *input_str_dval5 = neu_dvalue_new(NEU_DTYPE_STRING);
    neu_dvalue_set_move_string(input_str_dval5, test_string5);
    neu_string_val_t string_val5;
    neu_string_val_init(&string_val5, input_str_key5, input_str_dval5);

    /** set the sixth element of 1D array **/
    neu_string_t *input_str_key6 = neu_string_from_cstr((char *) "string-key6");
    neu_string_t *test_string6 =
        neu_string_from_cstr((char *) "hello,string-val6");
    neu_data_val_t *input_str_dval6 = neu_dvalue_new(NEU_DTYPE_STRING);
    neu_dvalue_set_move_string(input_str_dval6, test_string6);
    neu_string_val_t string_val6;
    neu_string_val_init(&string_val6, input_str_key6, input_str_dval6);

    /** set the seventh element of 1D array **/
    neu_string_t *input_str_key7 = neu_string_from_cstr((char *) "string-key7");
    neu_string_t *test_string7 =
        neu_string_from_cstr((char *) "hello,string-val7");
    neu_data_val_t *input_str_dval7 = neu_dvalue_new(NEU_DTYPE_STRING);
    neu_dvalue_set_move_string(input_str_dval7, test_string7);
    neu_string_val_t string_val7;
    neu_string_val_init(&string_val7, input_str_key7, input_str_dval7);

    /** set the eighth element of 1D array **/
    neu_string_t *input_str_key8 = neu_string_from_cstr((char *) "string-key8");
    neu_string_t *test_string8 =
        neu_string_from_cstr((char *) "hello,string-val8");
    neu_data_val_t *input_str_dval8 = neu_dvalue_new(NEU_DTYPE_STRING);
    neu_dvalue_set_move_string(input_str_dval8, test_string8);
    neu_string_val_t string_val8;
    neu_string_val_init(&string_val8, input_str_key8, input_str_dval8);

    /** set the first element of 2D array11 **/
    neu_fixed_array_t *set_str_val_array11 =
        neu_fixed_array_new(2, sizeof(neu_string_val_t));
    neu_fixed_array_set(set_str_val_array11, 0, &string_val1);
    neu_fixed_array_set(set_str_val_array11, 1, &string_val2);

    neu_data_val_t *array_str_val_dval11 = neu_dvalue_unit_new();
    neu_dvalue_init_ref_array(array_str_val_dval11, NEU_DTYPE_STRING_VAL,
                              set_str_val_array11);

    /** set the second element of 2D array12 **/
    neu_fixed_array_t *set_str_val_array12 =
        neu_fixed_array_new(2, sizeof(neu_string_val_t));
    neu_fixed_array_set(set_str_val_array12, 0, &string_val3);
    neu_fixed_array_set(set_str_val_array12, 1, &string_val4);

    neu_data_val_t *array_str_val_dval12 = neu_dvalue_unit_new();
    neu_dvalue_init_ref_array(array_str_val_dval12, NEU_DTYPE_STRING_VAL,
                              set_str_val_array12);

    /** set the first element of 2D array21 **/
    neu_fixed_array_t *set_str_val_array21 =
        neu_fixed_array_new(2, sizeof(neu_string_val_t));
    neu_fixed_array_set(set_str_val_array21, 0, &string_val5);
    neu_fixed_array_set(set_str_val_array21, 1, &string_val6);

    neu_data_val_t *array_str_val_dval21 = neu_dvalue_unit_new();
    neu_dvalue_init_ref_array(array_str_val_dval21, NEU_DTYPE_STRING_VAL,
                              set_str_val_array21);

    /** set the second element of 2D array22 **/
    neu_fixed_array_t *set_str_val_array22 =
        neu_fixed_array_new(2, sizeof(neu_string_val_t));
    neu_fixed_array_set(set_str_val_array22, 0, &string_val7);
    neu_fixed_array_set(set_str_val_array22, 1, &string_val8);

    neu_data_val_t *array_str_val_dval22 = neu_dvalue_unit_new();
    neu_dvalue_init_ref_array(array_str_val_dval22, NEU_DTYPE_STRING_VAL,
                              set_str_val_array22);

    /** set the first element of 3D array1 **/
    neu_fixed_array_t *set_str_val_array1 =
        neu_fixed_array_new(2, sizeof(neu_data_val_t *));
    neu_fixed_array_set(set_str_val_array1, 0, &array_str_val_dval11);
    neu_fixed_array_set(set_str_val_array1, 1, &array_str_val_dval12);

    neu_data_val_t *array_str_val_dval1 = neu_dvalue_unit_new();
    neu_dvalue_init_ref_array(array_str_val_dval1, NEU_DTYPE_DATA_VAL,
                              set_str_val_array1);

    /** set the second element of 3D array1 **/
    neu_fixed_array_t *set_str_val_array2 =
        neu_fixed_array_new(2, sizeof(neu_data_val_t *));
    neu_fixed_array_set(set_str_val_array2, 0, &array_str_val_dval21);
    neu_fixed_array_set(set_str_val_array2, 1, &array_str_val_dval22);

    neu_data_val_t *array_str_val_dval2 = neu_dvalue_unit_new();
    neu_dvalue_init_ref_array(array_str_val_dval2, NEU_DTYPE_DATA_VAL,
                              set_str_val_array2);

    /** set 3D array **/
    neu_fixed_array_t *set_array_str_val =
        neu_fixed_array_new(2, sizeof(neu_data_val_t *));
    neu_fixed_array_set(set_array_str_val, 0, &array_str_val_dval1);
    neu_fixed_array_set(set_array_str_val, 1, &array_str_val_dval2);

    /** serialize and deserializa return **/
    neu_fixed_array_t *get_array_str_val =
        ser_deser_with_fixed_array(set_array_str_val, NEU_DTYPE_DATA_VAL);

    /** free **/
    neu_string_free(input_str_key1);
    neu_string_free(input_str_key2);
    neu_string_free(input_str_key3);
    neu_string_free(input_str_key4);
    neu_string_free(input_str_key5);
    neu_string_free(input_str_key6);
    neu_string_free(input_str_key7);
    neu_string_free(input_str_key8);

    neu_dvalue_free(input_str_dval1);
    neu_dvalue_free(input_str_dval2);
    neu_dvalue_free(input_str_dval3);
    neu_dvalue_free(input_str_dval4);
    neu_dvalue_free(input_str_dval5);
    neu_dvalue_free(input_str_dval6);
    neu_dvalue_free(input_str_dval7);
    neu_dvalue_free(input_str_dval8);

    neu_fixed_array_free(set_str_val_array11);
    neu_fixed_array_free(set_str_val_array12);
    neu_fixed_array_free(set_str_val_array21);
    neu_fixed_array_free(set_str_val_array22);

    neu_dvalue_free(array_str_val_dval11);
    neu_dvalue_free(array_str_val_dval12);
    neu_dvalue_free(array_str_val_dval21);
    neu_dvalue_free(array_str_val_dval22);

    neu_fixed_array_free(set_str_val_array1);
    neu_fixed_array_free(set_str_val_array2);

    neu_dvalue_free(array_str_val_dval1);
    neu_dvalue_free(array_str_val_dval2);

    /** Compare **/
    comp_arr_3d_str_val(get_array_str_val);
}

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
