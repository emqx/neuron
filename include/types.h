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
 **/

#ifndef TYPES_H
#define TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>    
#include <stdbool.h>    
#include <stdint.h>    
    
/**
 * Order
 * -----
 * The Order enum is used to establish an absolute ordering between 
 * elements.
 **/
typedef enum {
    NP_ORDER_LESS = -1,
    NP_ORDER_EQ = 0,
    NP_ORDER_MORE = 1
} np_order_t;

/**
 * Boolean
 * -------
 * A logical value (true or false). 
 **/
typedef bool np_boolean_t;
#define NP_TRUE true
#define NP_FALSE false

/**
 * State
 * -----
 * A state value (on or off). 
 **/
typedef bool np_state_t;
#define NP_ON true
#define NP_OFF false

/**
 * Byte
 * ----
 * An integer value between -128 and 127. 
 **/
typedef int8_t np_byte_t;
#define NP_BYTE_MIN (-128)
#define NP_BYTE_MAX 127

/**
 * Unsigned-Byte
 * -------------
 * An integer value between 0 and 255. 
 **/
typedef uint8_t np_ubyte_t;
#define NP_UBYTE_MIN 0
#define NP_UBYTE_MAX 255

/**
 * Word
 * ----
 * An integer value between -32 768 and 32 767. 
 **/
typedef int16_t np_word_t;
#define NP_WORD_MIN (-32768)
#define NP_WORD_MAX 32767

/**
 * Unsigned-Word
 * -------------
 * An integer value between 0 and 65 535. 
 **/
typedef uint16_t np_uword_t;
#define NP_UWORD_MIN 0
#define NP_UWORD_MAX 65535

/**
 * Double Word
 * -----------
 * An integer value between -2 147 483 648 and 2 147 483 647. 
 **/
typedef int32_t np_dword_t;
#define NP_DWORD_MIN (-2147483648)
#define NP_DWORD_MAX 2147483647

/**
 * Unsigned Double Word
 * --------------------
 * An integer value between 0 and 4 294 967 295. 
 **/
typedef uint32_t np_udword_t;
#define NP_UDWORD_MIN 0
#define NP_UDWORD_MAX 4294967295

/**
 * Quarter Word
 * ------------
 * An integer value between -9 223 372 036 854 775 808 and
 * 9 223 372 036 854 775 807. 
 **/
typedef int64_t np_qword_t;
#define NP_QWORD_MIN (-9223372036854775808)
#define NP_QWORD_MAX 9223372036854775807

/**
 * Unsigned Quarter Word
 * ---------------------
 * An integer value between 0 and 18 446 744 073 709 551 615. 
 **/
typedef uint64_t np_uqword_t;
#define NP_UQWORD_MIN 0
#define NP_UQWORD_MAX 18446744073709551615

/**
 * Float
 * -----
 * An IEEE single precision (32 bit) floating point value. 
 **/
typedef float np_float_t;

/**
 * Double
 * ------
 * An IEEE double precision (64 bit) floating point value. 
 **/
typedef double np_double_t;

/**
 * Byte (decimal)
 * --------------
 * An integer value between -128 and 127 with decimal position. 
 **/
typedef struct {
    np_byte_t value;
    uint8_t decimal;
} np_byte_deci_t;

/**
 * Unsigned-Byte (decimal)
 * -----------------------
 * An integer value between 0 and 255 with decimal position. 
 **/
typedef struct {
    np_ubyte_t value;
    uint8_t decimal;
} np_ubyte_deci_t;

/**
 * Word
 * ----
 * An integer value between -32 768 and 32 767 with decimal position. 
 **/
typedef struct {
    np_word_t value;
    uint8_t decimal;
} np_word_deci_t;

/**
 * Unsigned-Word
 * -------------
 * An integer value between 0 and 65 535 with decimal position. 
 **/
typedef struct {
    np_uword_t value;
    uint8_t decimal;
} np_uword_deci_t;

/**
 * Double Word
 * -----------
 * An integer value between -2 147 483 648 and 2 147 483 647 with decimal position. 
 **/
typedef struct {
    np_dword_t value;
    uint8_t decimal;
} np_dword_deci_t;

/**
 * Unsigned Double Word
 * --------------------
 * An integer value between 0 and 4 294 967 295 with decimal position. 
 **/
typedef struct {
    np_udword_t value;
    uint8_t decimal;
} np_udword_deci_t;

/**
 * Quarter Word
 * ------------
 * An integer value between -9 223 372 036 854 775 808 and
 * 9 223 372 036 854 775 807 with decimal position. 
 **/
typedef struct {
    np_qword_t value;
    uint8_t decimal;
} np_qword_deci_t;

/**
 * Unsigned Quarter Word
 * ---------------------
 * An integer value between 0 and 18 446 744 073 709 551 615 with decimal position. 
 **/
typedef struct {
    np_uqword_t value;
    uint8_t decimal;
} np_uqword_deci_t;

/**
 * String
 * ------
 * A sequence of unicode characters. Strings are just an array of char. 
 **/
typedef struct {
    size_t length;
    char *charstr;
} np_string_t;

/* returns a string pointing to the original character array. */
np_string_t np_string_fromArray(char *array);

/* returns a copy of the character array content on the heap. */
np_string_t np_string_fromChars(const char *chars);

/* returns order type by comparing of two character arrays content. */
np_order_t np_string_isEqual(const np_string_t *s1, const np_string_t *s2);

extern const np_string_t NP_STRING_NULL;

/* Define strings at compile time */
#define np_string_fromStatic(CHARS) {sizeof(CHARS)-1, (char*)CHARS}

/**
 * Localized Text
 * --------------
 * Human readable text with an optional locale identifier. 
 **/
typedef struct {
    np_string_t locale;
    np_string_t text;
} np_text_t;

static inline np_text_t
np_text_fromArray(char *locale, char *array) {
    np_text_t t; 
    t.locale = np_string_fromArray(locale);
    t.text = np_string_fromArray(array); 
    return t;
}

static inline np_text_t
np_text_fromChars(const char *locale, const char *chars) {
    np_text_t t; 
    t.locale = np_string_fromChars(locale);
    t.text = np_string_fromChars(chars); 
    return t;
}

/**
 * ErrorCode
 * ---------
 * A numeric identifier for an error or condition that is associated with a value
 * or an operation. See the section :ref:`statuscodes` for the meaning of a
 * specific code. 
 **/
typedef uint32_t np_errorcode_t;

/* Returns the human-readable name of the StatusCode. If no matching StatusCode
 * is found, a default string for "Unknown" is returned.*/
const char *np_errorcode_getName(np_errorcode_t code);

/**
 * Time
 * ----
 * An instance in time. A DateTime value is encoded as a 32-bit signed integer
 * which represents the number of one second interval since January 1, 1970
 * (UTC).
 **/
typedef uint32_t np_time_t;

/* The current time in local time */
np_time_t np_time_now(void);

/* Represents a Datetime as a structure */
typedef struct tm np_datetime_t;

/* Returns the datetime structure from time */
np_datetime_t np_time_getDateTime(np_time_t t);

/* Returns the instance time from datetime structure */
np_time_t np_time_fromDateTime(np_datetime_t d);

/**
 * Uuid
 * ----
 * A 16 byte value that can be used as a universal unique identifier. 
 **/
typedef struct {
    np_udword_t data1;
    np_uword_t data2;
    np_uword_t data3;
    np_ubyte_t data4[8];
} np_uuid_t;

/* Parse the uuid format defined in Part 6, 5.1.3.
 * Format: C496578A-0DFE-4B8F-870A-745238C6AEAE
 *         |       |    |    |    |            |
 *         0       8    13   18   23           36 */
np_boolean_t np_uuid_isEqual(const np_uuid_t *g1, const np_uuid_t *g2);

extern const np_uuid_t NP_UUID_NULL;

/**
 * NodeId
 * ^^^^^^
 * An identifier for a node to be located in the list. 
 **/
typedef struct {
    np_text_t nodeName;
    np_string_t identifier;
} np_nodeid_t;

np_boolean_t np_nodeid_isNull(const np_nodeid_t *p);

np_order_t np_nodeid_order(const np_nodeid_t *n1, const np_nodeid_t *n2);

/**
 * Data Type
 * ---------
 * A list of data type 
 **/
typedef enum {
    NP_DATATYPE_BOOLEAN,
    NP_DATATYPE_STATE,
    NP_DATATYPE_BYTE,
    NP_DATATYPE_UBYTE,
    NP_DATATYPE_WORD,
    NP_DATATYPE_UWORD,
    NP_DATATYPE_DWORD,
    NP_DATATYPE_UDWORD,
    NP_DATATYPE_QWORD,
    NP_DATATYPE_UQWORD,
    NP_DATATYPE_FLOAT,
    NP_DATATYPE_DOUBLE,
    NP_DATATYPE_BYTE_DECI,
    NP_DATATYPE_UBYTE_DECI,
    NP_DATATYPE_WORD_DECI,
    NP_DATATYPE_UWORD_DECI,
    NP_DATATYPE_DWORD_DECI,
    NP_DATATYPE_UDWORD_DECI,
    NP_DATATYPE_QWORD_DECI,
    NP_DATATYPE_UQWORD_DECI,
    NP_DATATYPE_STRING,
    NP_DATATYPE_LOCALIZEDTEXT,
    NP_DATATYPE_ERRORCODE,
    NP_DATATYPE_DATETIME,
    NP_DATATYPE_UUID,
    NP_DATATYPE_ADDRESSELEMENT,
    NP_DATATYPE_ADDRESS,
    NP_DATATYPE_TAG,
    NP_DATATYPE_PARAMETER,
    NP_DATATYPE_MODULE,
    NP_DATATYPE_VARIABLE,
    NP_DATATYPE_VARIABLENODE,
    NP_DATATYPE_COUNT
} np_datatype_t;

/* Test if the data type is a numeric builtin data type. This includes Boolean,
 * integers and floating point numbers. Not included are Time and ErrorCode. */
np_boolean_t
np_datatype_isNumeric(const np_datatype_t *type);

struct np_variabletype {
    np_text_t *typeName;
    np_datatype_t typeId;                /* The nodeid of the type */
    np_uword_t memSize;               /* Size of the struct in memory */
};

/**
 * Address Element
 * ---------------
 * An address element is a basic information unit that represent partly address 
 * location.
 **/
typedef enum {
    NP_ADDRESSELEMENTTYPE_HEADER,
    NP_ADDRESSELEMENTTYPE_UUID,
    NP_ADDRESSELEMENTTYPE_IDENTIFIER,
    NP_ADDRESSELEMENTTYPE_STATION,
    NP_ADDRESSELEMENTTYPE_NETWORKNO,
    NP_ADDRESSELEMENTTYPE_SOURCE,
    NP_ADDRESSELEMENTTYPE_DESTINATION,
    NP_ADDRESSELEMENTTYPE_REGISTER,
    NP_ADDRESSELEMENTTYPE_NAMESPACEINDEX,
    NP_ADDRESSELEMENTTYPE_NAMESPACE,
    NP_ADDRESSELEMENTTYPE_OBJECTID,
    NP_ADDRESSELEMENTTYPE_ATTRIBUTEID
} np_addresselementtype_t;

typedef struct {
    np_addresselementtype_t type;
    np_boolean_t isNumeric;
    np_string_t element;
} np_addresselement_t;

/**
 * Address
 * -------
 * An address is a combination of address element in an order format to locate 
 * the data storage in external device.
 **/
typedef enum {
    NP_ADDRESSTYPE_REGISTER,
    NP_ADDRESSTYPE_SYMBOLIC,
    NP_ADDRESSTYPE_TOPIC,
    NP_ADDRESSTYPE_APIPATH
} np_addresstype_t;

typedef struct {
    np_addresstype_t type;
    np_datatype_t dataType;
    np_byte_t delimiter;
    size_t elementSize;
    np_addresselement_t *elements;
} np_address_t;

/**
 * Tag
 * ---
 * An attribute is a basic communicatoin element that describe how to interact
 * with external device. 
 **/
typedef enum {
    NP_ATTRIBUTETYPE_READ,
    NP_ATTRIBUTETYPE_WRITE,
    NP_ATTRIBUTETYPE_SUBSCRIBE
} np_attributetype_t;  

typedef struct {
    np_attributetype_t type;
    np_word_t readInterval;
    np_datatype_t dataType;
    np_address_t address;
} np_tag_t;

/**
 * Parameter
 * ---------
 * A paramter is a communicaton details for connection setup and data format 
 * description.
 **/
typedef struct {
    np_text_t paramDesc;
    np_string_t paramName;
    np_string_t paramData;
} np_parameter_t;

/**
 * Module
 * ------
 * A module is a communication dynamic library. It may contains many paramters 
 * used for communication setup. Each module can be used to create more than one 
 * communication channel.
 **/
typedef struct {
    np_ubyte_t version;
    np_string_t moduleName;
    np_text_t moduleDesc;
    size_t sizeParam;
    np_parameter_t *param;
    np_string_t path;
} np_module_t;

/**
 * Variable
 * --------
 *
 * Variable may contain values of any type together with a description of the
 * content. The standard mandates that variable contain built-in data types only.
 *
 * Variable may contain a scalar value or an array. Array variable can have
 * an additional dimensionality defined in an array of dimension lengths. The 
 * actual values are kept in an array of dimensions one.
 **/
struct np_variabletype;
typedef struct np_variabletype np_variabletype_t;

typedef struct {
    const np_variabletype_t *type;      /* The data type description */
    size_t arrayLength;           /* The number of elements in the data array */
    size_t arrayDimensionsSize;   /* The number of dimensions */
    np_udword_t *arrayDimensions;   /* The length of each dimension */
    void *data;                   /* Points to the scalar or array data */
} np_variable_t;

/* Returns true if the variable has no value defined (contains neither an array
 * nor a scalar value). */
static inline np_boolean_t
np_variable_isEmpty(const np_variable_t *var) {
    return var->type == NULL;
}

/* Returns true if the variable contains a scalar value. Note that empty variable
 * contain an array of length -1 (undefined). */
static inline np_boolean_t
np_variable_isScalar(const np_variable_t *var) {
    return (var->arrayLength == 0 && var->data != NULL);
}

/* Returns true if the variant contains a scalar value of the given type. */
static inline np_boolean_t
np_variable_hasScalarType(const np_variable_t *var, const np_variabletype_t *type) {
    return np_variable_isScalar(var) && type == var->type;
}

/* Returns true if the variable contains an array of the given type. */
static inline np_boolean_t
np_variable_hasArrayType(const np_variable_t *var, const np_variabletype_t *type) {
    return (!np_variable_isScalar(var)) && type == var->type;
}

/* Set the variable to a scalar value that already resides in memory. The value
 * takes on the lifecycle of the variable and is deleted with it. */
void
np_variable_setScalar(np_variable_t *var, void *p, 
                            const np_variabletype_t *type);

/* Set the variable to a scalar value that is copied from an existing variable. */
np_errorcode_t
np_variable_setScalarCopy(np_variable_t *var, const void *p,
                            const np_variabletype_t *type);

/* Set the variable to an array that already resides in memory. The array takes
 * on the lifecycle of the variant and is deleted with it. */
void
np_variable_setArray(np_variable_t *var, void *array,
                    size_t arraySize, const np_variabletype_t *type);

/* Set the variable to an array that is copied from an existing array. */
np_errorcode_t
np_variable_setArrayCopy(np_variable_t *var, const void *array,
                        size_t arraySize, const  np_variabletype_t *type);


/**
 * Variable Node
 * -------------
 * A variable node is a variable with node id that can be managed in a list. All 
 * variables are mandated to have node id to identify itself for the adaptors.
 **/
typedef struct {
    np_nodeid_t nodeid;
    np_variable_t variable;
} np_varaiblenode_t;



#ifdef __cplusplus
}
#endif

#endif /* TYPES_H */

