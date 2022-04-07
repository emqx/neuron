/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2021 EMQ Technologies Co., Ltd All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
config_ **/

#ifndef NEURON_TYPES_H
#define NEURON_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "log.h"

/**
 * Order
 * -----
 * The Order enum is used to establish an absolute ordering between
 * elements.
 **/
typedef enum {
    NEU_ORDER_LESS = -1,
    NEU_ORDER_EQ   = 0,
    NEU_ORDER_MORE = 1
} neu_order_t;

/**
 * Check if two value is equal.
 */
typedef bool (*neu_equal_func)(const void *a, const void *b);

/**
 * Compare two value.
 */
typedef neu_order_t (*neu_compare_func)(const void *a, const void *b);

/**
 * Check if the key is match the item
 */
typedef bool (*neu_key_match_func)(const void *key, const void *item);

/*
 * define functions for implement equal function for primitive value
 */
#define gen_prim_val_equal(locase_type, locase_type_t)                   \
    static inline bool locase_type##_equal(const void *a, const void *b) \
    {                                                                    \
        return *(locase_type_t *) (a) == *(locase_type_t *) (b);         \
    }

#define gen_prim_val_equal_t(locase_type) \
    gen_prim_val_equal(locase_type, locase_type##_t)

gen_prim_val_equal_t(int8);  // generate function int8_equal
gen_prim_val_equal_t(int16); // generate function int16_equal
gen_prim_val_equal_t(int32); // generate function int32_equal
gen_prim_val_equal_t(int64); // generate function int64_equal

gen_prim_val_equal_t(uint8);  // generate function uint8_equal
gen_prim_val_equal_t(uint16); // generate function uint16_equal
gen_prim_val_equal_t(uint32); // generate function uint32_equal
gen_prim_val_equal_t(uint64); // generate function uint64_equal

gen_prim_val_equal(float, float);   // generate function float_equal
gen_prim_val_equal(double, double); // generate function double_equal

/*
 * define functions for implement compare function for primitive value
 */
#define gen_prim_val_compare(locase_type, locase_type_t)                   \
    static inline bool locase_type##_compare(const void *a, const void *b) \
    {                                                                      \
        locase_type_t val_a = *(locase_type_t *) (a);                      \
        locase_type_t val_b = *(locase_type_t *) (b);                      \
        return val_a == val_b                                              \
            ? NEU_ORDER_EQ                                                 \
            : val_a > val_b ? NEU_ORDER_MORE : NEU_ORDER_LESS;             \
    }

#define gen_prim_val_compare_t(locase_type) \
    gen_prim_val_compare(locase_type, locase_type##_t)

gen_prim_val_compare_t(int8);  // generate function int8_compare
gen_prim_val_compare_t(int16); // generate function int16_compare
gen_prim_val_compare_t(int32); // generate function int32_compare
gen_prim_val_compare_t(int64); // generate function int64_compare

gen_prim_val_compare_t(uint8);  // generate function uint8_compare
gen_prim_val_compare_t(uint16); // generate function uint16_compare
gen_prim_val_compare_t(uint32); // generate function uint32_compare
gen_prim_val_compare_t(uint64); // generate function uint64_compare

gen_prim_val_compare(float, float);   // generate function float_compare
gen_prim_val_compare(double, double); // generate function double_compare

/**
 * Boolean
 * -------
 * A logical value (true or false).
 **/
typedef bool neu_boolean_t;
#define NEU_TRUE true
#define NEU_FALSE false

/**
 * State
 * -----
 * A state value (on or off).
 **/
typedef bool neu_state_t;
#define NEU_ON true
#define NEU_OFF false

/**
 * Byte
 * ----
 * An integer value between -128 and 127.
 **/
typedef int8_t neu_byte_t;
#define NEU_BYTE_MIN (-128)
#define NEU_BYTE_MAX 127

/**
 * Unsigned-Byte
 * -------------
 * An integer value between 0 and 255.
 **/
typedef uint8_t neu_ubyte_t;
#define NEU_UBYTE_MIN 0
#define NEU_UBYTE_MAX 255

/**
 * Word
 * ----
 * An integer value between -32 768 and 32 767.
 **/
typedef int16_t neu_word_t;
#define NEU_WORD_MIN (-32768)
#define NEU_WORD_MAX 32767

/**
 * Unsigned-Word
 * -------------
 * An integer value between 0 and 65 535.
 **/
typedef uint16_t neu_uword_t;
#define NEU_UWORD_MIN 0
#define NEU_UWORD_MAX 65535

/**
 * Double Word
 * -----------
 * An integer value between -2 147 483 648 and 2 147 483 647.
 **/
typedef int32_t neu_dword_t;
#define NEU_DWORD_MIN (-2147483648)
#define NEU_DWORD_MAX 2147483647

/**
 * Unsigned Double Word
 * --------------------
 * An integer value between 0 and 4 294 967 295.
 **/
typedef uint32_t neu_udword_t;
#define NEU_UDWORD_MIN 0
#define NEU_UDWORD_MAX 4294967295

/**
 * Quarter Word
 * ------------
 * An integer value between -9 223 372 036 854 775 808 and
 * 9 223 372 036 854 775 807.
 **/
typedef int64_t neu_qword_t;
#define NEU_QWORD_MIN (-9223372036854775808)
#define NEU_QWORD_MAX 9223372036854775807

/**
 * Unsigned Quarter Word
 * ---------------------
 * An integer value between 0 and 18 446 744 073 709 551 615.
 **/
typedef uint64_t neu_uqword_t;
#define NEU_UQWORD_MIN 0
#define NEU_UQWORD_MAX 18446744073709551615

/**
 * Float
 * -----
 * An IEEE single precision (32 bit) floating point value.
 **/
typedef float neu_float_t;

/**
 * Double
 * ------
 * An IEEE double precision (64 bit) floating point value.
 **/
typedef double neu_double_t;

/* decimal point range */
#define NEU_DECIMAL_MIN 0
#define NEU_DECIMAL_MAX 5

/**
 * Byte (decimal)
 * --------------
 * An integer value between -128 and 127 with decimal position.
 **/
typedef struct {
    neu_byte_t value;
    uint8_t    decimal;
} neu_bytedec_t;

neu_double_t neu_bytedec_toDouble(neu_bytedec_t b);

/**
 * Unsigned-Byte (decimal)
 * -----------------------
 * An integer value between 0 and 255 with decimal position.
 **/
typedef struct {
    neu_ubyte_t value;
    uint8_t     decimal;
} neu_ubytedec_t;

neu_double_t neu_ubytedec_toDouble(neu_ubytedec_t ub);

/**
 * Word
 * ----
 * An integer value between -32 768 and 32 767 with decimal position.
 **/
typedef struct {
    neu_word_t value;
    uint8_t    decimal;
} neu_worddec_t;

neu_double_t neu_worddec_toDouble(neu_worddec_t w);

/**
 * Unsigned-Word
 * -------------
 * An integer value between 0 and 65 535 with decimal position.
 **/
typedef struct {
    neu_uword_t value;
    uint8_t     decimal;
} neu_uworddec_t;

neu_double_t neu_uworddec_toDouble(neu_uworddec_t uw);

/**
 * Double Word
 * -----------
 * An integer value between -2 147 483 648 and 2 147 483 647 with decimal
 *position.
 **/
typedef struct {
    neu_dword_t value;
    uint8_t     decimal;
} neu_dworddec_t;

neu_double_t neu_dworddec_toDouble(neu_dworddec_t dw);

/**
 * Unsigned Double Word
 * --------------------
 * An integer value between 0 and 4 294 967 295 with decimal position.
 **/
typedef struct {
    neu_udword_t value;
    uint8_t      decimal;
} neu_udworddec_t;

neu_double_t neu_udworddec_toDouble(neu_udworddec_t udw);

/**
 * Quarter Word
 * ------------
 * An integer value between -9 223 372 036 854 775 808 and
 * 9 223 372 036 854 775 807 with decimal position.
 **/
typedef struct {
    neu_qword_t value;
    uint8_t     decimal;
} neu_qworddec_t;

neu_double_t neu_qworddec_toDouble(neu_qworddec_t qw);

/**
 * Unsigned Quarter Word
 * ---------------------
 * An integer value between 0 and 18 446 744 073 709 551 615 with decimal
 *position.
 **/
typedef struct {
    neu_uqword_t value;
    uint8_t      decimal;
} neu_uqworddec_t;

neu_double_t neu_uqworddec_toDouble(neu_uqworddec_t uqw);

/**
 * String
 * ------
 * A sequence of unicode characters. Strings are just an array of char.
 **/
typedef struct neu_string neu_string_t;

/**
 * memory size of the string
 */
size_t        neu_string_size(neu_string_t *string);
size_t        neu_string_serialized_size(neu_string_t *string);
neu_string_t *neu_string_new(size_t length);
void          neu_string_free(neu_string_t *string);
void          neu_string_copy(neu_string_t *dst, neu_string_t *src);
neu_string_t *neu_string_clone(neu_string_t *string);

/* returns a string from cstr with '\0' */
neu_string_t *neu_string_from_cstr(char *cstr);

/* Get a reference of cstr in string */
char *neu_string_get_ref_cstr(neu_string_t *string);

/* returns order type by comparing of two character arrays content. */
neu_order_t neu_string_cmp(const neu_string_t *s1, const neu_string_t *s2);

size_t neu_string_serialized_size(neu_string_t *string);

/* serialize the neu_sting_t to bytes buffer */
size_t neu_string_serialize(neu_string_t *string, uint8_t *buf);

/* deserialize the neu_sting_t from bytes buffer */
size_t neu_string_deserialize(uint8_t *buf, neu_string_t **p_string);

/**
 * Localized Text
 * --------------
 * Human readable text with an optional locale identifier.
 **/
typedef struct {
    neu_string_t *locale;
    neu_string_t *text;
} neu_text_t;

static inline neu_text_t neu_text_from_cstr(char *locale, char *cstr)
{
    neu_text_t t;
    t.locale = neu_string_from_cstr(locale);
    t.text   = neu_string_from_cstr(cstr);
    return t;
}

static inline void neu_text_free(neu_text_t *text)
{
    if (text != NULL) {
        neu_string_free(text->locale);
        neu_string_free(text->text);
    }

    return;
}

/**
 * Bytes
 * ------
 * A sequence of byte. Bytes are just an array of byte(uint8_t).
 **/
typedef struct {
    size_t  length;
    uint8_t buf[0];
} neu_bytes_t;

/**
 * memory size of the bytes
 */
static inline size_t neu_bytes_size(neu_bytes_t *bytes)
{
    return sizeof(size_t) + bytes->length;
}

/**
 * New a bytes by length
 */
static inline neu_bytes_t *neu_bytes_new(size_t length)
{
    size_t       size;
    neu_bytes_t *bytes;

    size  = sizeof(size_t) + length;
    bytes = (neu_bytes_t *) malloc(size);
    if (bytes == NULL) {
        return NULL;
    }

    bytes->length = length;
    return bytes;
}

static inline neu_bytes_t *neu_bytes_from_buf(uint8_t *buf, size_t length)
{
    neu_bytes_t *bytes;

    bytes = neu_bytes_new(length);
    if (bytes == NULL) {
        return NULL;
    }

    memcpy(bytes->buf, buf, length);
    return bytes;
}

/**
 * shallow free
 */
static inline void neu_bytes_free(neu_bytes_t *bytes)
{
    free(bytes);
}

static inline void neu_bytes_copy(neu_bytes_t *dst, neu_bytes_t *src)
{
    size_t size;

    size = neu_bytes_size(src);
    memcpy(dst, src, size);
    return;
}

static inline neu_bytes_t *neu_bytes_clone(neu_bytes_t *bytes)
{
    neu_bytes_t *new_bytes;
    size_t       size;

    size      = neu_bytes_size(bytes);
    new_bytes = (neu_bytes_t *) malloc(size);
    if (new_bytes == NULL) {
        return NULL;
    }

    new_bytes->length = bytes->length;
    memcpy(new_bytes->buf, bytes->buf, new_bytes->length);
    return new_bytes;
}

/**
 * Fixed Array
 * ------
 * A sequence of structure element.
 **/
#define FIXED_ARRAY_ESIZE_MAX ((1 << 16) - 1)

struct neu_fixed_array {
    size_t length;
    size_t esize; ///< element size

    // For align with 8 bytes
    uint64_t buf[0];
    // Don’t add any member after here
};

typedef struct neu_fixed_array neu_fixed_array_t;

/**
 * memory size of the fixed array
 */
static inline size_t neu_fixed_array_size(neu_fixed_array_t *array)
{
    size_t size;

    size = array->length * array->esize + 2 * sizeof(size_t);
    return size;
}

/**
 * New a fixed array by length and element size.
 * NOTE: the esize must less then 64K Byte
 */
neu_fixed_array_t *neu_fixed_array_new(size_t length, size_t esize);

/**
 * Get a element in fixed array, return a pointer of element
 */
static inline void *neu_fixed_array_get(neu_fixed_array_t *array, size_t index)
{
    return (uint8_t *) array->buf + array->esize * index;
}

/**
 * Set a element into fixed array, parameter is a pointer of element
 */
static inline void neu_fixed_array_set(neu_fixed_array_t *array, size_t index,
                                       void *elem)
{
    uint8_t *elem_ptr;

    elem_ptr = (uint8_t *) array->buf + array->esize * index;
    memcpy(elem_ptr, elem, array->esize);
    return;
}

/**
 * shallow free
 */
void neu_fixed_array_free(neu_fixed_array_t *array);

/**
 * shallow copy
 */
void neu_fixed_array_copy(neu_fixed_array_t *dst, neu_fixed_array_t *src);

/**
 * shallow clone
 */
neu_fixed_array_t *neu_fixed_array_clone(neu_fixed_array_t *array);

/**
 * ErrorCode
 * ---------
 * A numeric identifier for an error or condition that is associated with a
 *value or an operation. See the section :ref:`statuscodes` for the meaning of a
 * specific code.
 **/
typedef uint32_t neu_errorcode_t;

/* Returns the human-readable name of the StatusCode. If no matching StatusCode
 * is found, a default string for "Unknown" is returned.*/
const char *neu_errorcode_getName(neu_errorcode_t code);

/**
 * Time
 * ----
 * An instance in time. A DateTime value is encoded as a 32-bit signed integer
 * which represents the number of one second interval since January 1, 1970
 * (UTC).
 **/
typedef uint32_t neu_time_t;

/* The current time in local time */
neu_time_t neu_time_now(void);

/* Represents a Datetime as a structure */
typedef struct tm neu_datetime_t;

/* Returns the datetime structure from time */
neu_datetime_t neu_time_getDateTime(neu_time_t t);

/* Returns the instance time from datetime structure */
neu_time_t neu_time_fromDateTime(neu_datetime_t d);

/**
 * Uuid
 * ----
 * A 16 byte value that can be used as a universal unique identifier.
 **/
typedef struct {
    neu_udword_t data1;
    neu_uword_t  data2;
    neu_uword_t  data3;
    neu_ubyte_t  data4[8];
} neu_uuid_t;

/* Parse the uuid format defined in Part 6, 5.1.3.
 * Format: C496578A-0DFE-4B8F-870A-745238C6AEAE
 *         |       |    |    |    |            |
 *         0       8    13   18   23           36 */
neu_boolean_t neu_uuid_isEqual(const neu_uuid_t *g1, const neu_uuid_t *g2);

extern const neu_uuid_t NEU_UUID_NULL;

/**
 * NodeId
 * ^^^^^^
 * An identifier for a node to be located in the list.
 **/
typedef struct {
    neu_text_t    nodeName;
    neu_string_t *identifier;
} neu_nodeid_t;

neu_boolean_t neu_nodeid_isNull(const neu_nodeid_t *p);

neu_order_t neu_nodeid_order(const neu_nodeid_t *n1, const neu_nodeid_t *n2);

/**
 * Data Type
 * ---------
 * A list of data type
 **/
typedef enum {
    NEU_DATATYPE_ERROR = 0,
    NEU_DATATYPE_ARRAY,
    NEU_DATATYPE_BOOLEAN,
    NEU_DATATYPE_STATE,
    NEU_DATATYPE_BYTE,
    NEU_DATATYPE_UBYTE,
    NEU_DATATYPE_WORD,
    NEU_DATATYPE_UWORD,
    NEU_DATATYPE_DWORD,
    NEU_DATATYPE_UDWORD,
    NEU_DATATYPE_QWORD,
    NEU_DATATYPE_UQWORD,
    NEU_DATATYPE_FLOAT,
    NEU_DATATYPE_DOUBLE,
    NEU_DATATYPE_BYTE_DECI,
    NEU_DATATYPE_UBYTE_DECI,
    NEU_DATATYPE_WORD_DECI,
    NEU_DATATYPE_UWORD_DECI,
    NEU_DATATYPE_DWORD_DECI,
    NEU_DATATYPE_UDWORD_DECI,
    NEU_DATATYPE_QWORD_DECI,
    NEU_DATATYPE_UQWORD_DECI,
    NEU_DATATYPE_STRING,
    NEU_DATATYPE_BYTES,
    NEU_DATATYPE_LOCALIZEDTEXT,
    NEU_DATATYPE_ERRORCODE,
    NEU_DATATYPE_DATETIME,
    NEU_DATATYPE_UUID,
    NEU_DATATYPE_ADDRESSELEMENT,
    NEU_DATATYPE_ADDRESS,
    NEU_DATATYPE_TAG,
    NEU_DATATYPE_PARAMETER,
    NEU_DATATYPE_MODULE,
    NEU_DATATYPE_VARIABLE,
    NEU_DATATYPE_VARIABLENODE,
    NEU_DATATYPE_COUNT
} neu_datatype_t;

/* Test if the data type is a numeric builtin data type. This includes Boolean,
 * integers and floating point numbers. Not included are Time and ErrorCode. */
neu_boolean_t neu_datatype_isNumeric(const neu_datatype_t *type);

struct neu_variabletype {
    neu_text_t *   typeName;
    neu_datatype_t typeId;  /* The nodeid of the type */
    neu_uword_t    memSize; /* Size of the struct in memory */
};

/**
 * Address Element
 * ---------------
 * An address element is a basic information unit that represent partly address
 * location.
 **/
/*
typedef enum {
    NEU_ADDRESSELEMENTTYPE_HEADER,
    NEU_ADDRESSELEMENTTYPE_UUID,
    NEU_ADDRESSELEMENTTYPE_IDENTIFIER,
    NEU_ADDRESSELEMENTTYPE_STATION,
    NEU_ADDRESSELEMENTTYPE_NETWORKNO,
    NEU_ADDRESSELEMENTTYPE_SOURCE,
    NEU_ADDRESSELEMENTTYPE_DESTINATION,
    NEU_ADDRESSELEMENTTYPE_REGISTER,
    NEU_ADDRESSELEMENTTYPE_NAMESPACEINDEX,
    NEU_ADDRESSELEMENTTYPE_NAMESPACE,
    NEU_ADDRESSELEMENTTYPE_OBJECTID,
    NEU_ADDRESSELEMENTTYPE_ATTRIBUTEID
} neu_addresselementtype_t;

typedef struct {
    neu_addresselementtype_t type;
    neu_boolean_t            isNumeric;
    neu_string_t             element;
} neu_addresselement_t;
*/

/**
 * Address
 * -------
 * An address is a combination of address element in an order format to locate
 * the data storage in external device.
 **/
/*
typedef enum {
    NEU_ADDRESSTYPE_REGISTER,
    NEU_ADDRESSTYPE_SYMBOLIC,
    NEU_ADDRESSTYPE_TOPIC,
    NEU_ADDRESSTYPE_APIPATH
} neu_addresstype_t;

typedef struct {
    neu_addresstype_t     type;
    neu_datatype_t        dataType;
    neu_byte_t            delimiter;
    size_t                elementSize;
    neu_addresselement_t *elements;
} neu_address_t;
*/

/**
 * Tag
 * ---
 * An attribute is a basic communicatoin element that describe how to interact
 * with external device.
 **/

/**
 * Parameter
 * ---------
 * A paramter is a communicaton details for connection setup and data format
 * description.
 **/
/*
typedef struct {
    neu_text_t   paramDesc;
    neu_string_t paramName;
    neu_string_t paramData;
} neu_parameter_t;
*/

/**
 * Module
 * ------
 * A module is a communication dynamic library. It may contains many paramters
 * used for communication setup. Each module can be used to create more than one
 * communication channel.
 **/
/*
typedef struct {
    neu_ubyte_t      version;
    neu_string_t     moduleName;
    neu_text_t       moduleDesc;
    size_t           sizeParam;
    neu_parameter_t *param;
    neu_string_t     path;
} neu_module_t;
*/

/**
 * Variable
 * --------
 *
 * Variable may contain values of any type together with a description of the
 * content. The standard mandates that variable contain built-in data types
 *only.
 *
 * Variable may contain a scalar value or an array. Array variable can have
 * an additional dimensionality defined in an array of dimension lengths. The
 * actual values are kept in an array of dimensions one.
 **/
struct neu_variabletype;
typedef struct neu_variabletype neu_variabletype_t;

typedef struct neu_variable_t {
    struct neu_variable_t *   next;
    struct neu_variable_t *   prev;
    int                       error;
    neu_datatype_t            v_type;
    size_t                    key_len;
    char *                    key;
    size_t                    data_len;
    void *                    data; /* Points to the scalar or array data */
    const neu_variabletype_t *type; /* The data type description */
    neu_variabletype_t        var_type;
    size_t        arrayLength; /* The number of elements in the data array */
    size_t        arrayDimensionsSize; /* The number of dimensions */
    neu_udword_t *arrayDimensions;     /* The length of each dimension */
} neu_variable_t;

/* Returns true if the variable has no value defined (contains neither an array
 * nor a scalar value). */
static inline neu_boolean_t neu_variable_isEmpty(const neu_variable_t *var)
{
    return var->type == NULL;
}

/* Returns true if the variable contains a scalar value. Note that empty
 * variable contain an array of length -1 (undefined). */
static inline neu_boolean_t neu_variable_isScalar(const neu_variable_t *var)
{
    return (var->arrayLength == 0 && var->data != NULL);
}

/* Returns true if the variant contains a scalar value of the given type. */
static inline neu_boolean_t
neu_variable_hasScalarType(const neu_variable_t *    var,
                           const neu_variabletype_t *type)
{
    return neu_variable_isScalar(var) && type == var->type;
}

/* Returns true if the variable contains an array of the given type. */
static inline neu_boolean_t
neu_variable_hasArrayType(const neu_variable_t *    var,
                          const neu_variabletype_t *type)
{
    return (!neu_variable_isScalar(var)) && type == var->type;
}

/* Set the variable to a scalar value that already resides in memory. The value
 * takes on the lifecycle of the variable and is deleted with it. */
void neu_variable_setScalar(neu_variable_t *var, void *p,
                            const neu_variabletype_t *type);

/* Set the variable to a scalar value that is copied from an existing variable.
 */
neu_errorcode_t neu_variable_setScalarCopy(neu_variable_t *var, const void *p,
                                           const neu_variabletype_t *type);

/* Set the variable to an array that already resides in memory. The array takes
 * on the lifecycle of the variant and is deleted with it. */
void neu_variable_setArray(neu_variable_t *var, void *array, size_t arraySize,
                           const neu_variabletype_t *type);

/* Set the variable to an array that is copied from an existing array. */
neu_errorcode_t neu_variable_setArrayCopy(neu_variable_t *var,
                                          const void *array, size_t arraySize,
                                          const neu_variabletype_t *type);

void neu_variable_setArray(neu_variable_t *v, void *array, size_t arraySize,
                           const neu_variabletype_t *type);

neu_variable_t *neu_variable_create();
neu_datatype_t  neu_variable_get_type(neu_variable_t *v);
void            neu_variable_set_key(neu_variable_t *v, const char *key);
int             neu_variable_get_key(neu_variable_t *v, char **key);
void            neu_variable_set_error(neu_variable_t *v, const int code);
int             neu_variable_get_error(neu_variable_t *v);
int             neu_variable_set_byte(neu_variable_t *v, const int8_t value);
int             neu_variable_get_byte(neu_variable_t *v, int8_t *value);
int             neu_variable_set_ubyte(neu_variable_t *v, const uint8_t value);
int             neu_variable_get_ubyte(neu_variable_t *v, uint8_t *value);
int             neu_variable_set_boolean(neu_variable_t *v, const bool value);
int             neu_variable_get_boolean(neu_variable_t *v, bool *value);
int             neu_variable_set_word(neu_variable_t *v, const int16_t value);
int             neu_variable_get_word(neu_variable_t *v, int16_t *value);
int             neu_variable_set_uword(neu_variable_t *v, const uint16_t value);
int             neu_variable_get_uword(neu_variable_t *v, uint16_t *value);
int             neu_variable_set_dword(neu_variable_t *v, const int32_t value);
int             neu_variable_get_dword(neu_variable_t *v, int32_t *value);
int    neu_variable_set_udword(neu_variable_t *v, const uint32_t value);
int    neu_variable_get_udword(neu_variable_t *v, uint32_t *value);
int    neu_variable_set_qword(neu_variable_t *v, const int64_t value);
int    neu_variable_get_qword(neu_variable_t *v, int64_t *value);
int    neu_variable_set_uqword(neu_variable_t *v, const uint64_t value);
int    neu_variable_get_uqword(neu_variable_t *v, uint64_t *value);
int    neu_variable_set_float(neu_variable_t *v, const float value);
int    neu_variable_get_float(neu_variable_t *v, float *value);
int    neu_variable_set_double(neu_variable_t *v, const double value);
int    neu_variable_get_double(neu_variable_t *v, double *value);
int    neu_variable_set_string(neu_variable_t *v, const char *value);
int    neu_variable_get_string(neu_variable_t *v, char **value, size_t *len);
int    neu_variable_add_item(neu_variable_t *v_array, neu_variable_t *v_item);
int    neu_variable_get_item(neu_variable_t *v_array, const int index,
                             neu_variable_t **v_item);
size_t neu_variable_count(neu_variable_t *v_array);
neu_variable_t *neu_variable_next(neu_variable_t *v);
void            neu_variable_destroy(neu_variable_t *v);
size_t          neu_variable_serialize(neu_variable_t *v, void **buf);
void            neu_variable_serialize_free(void *buf);
neu_variable_t *neu_variable_deserialize(void *buf, size_t buf_len);
const char *    neu_variable_get_str(neu_variable_t *v);
/**
 * Variable Node
 * -------------
 * A variable node is a variable with node id that can be managed in a list. All
 * variables are mandated to have node id to identify itself for the adaptors.
 **/
typedef struct {
    neu_nodeid_t   nodeid;
    neu_variable_t variable;
} neu_varaiblenode_t;

int string_is_number(char *s);

#ifdef __cplusplus
}
#endif

#endif /* TYPES_H */
