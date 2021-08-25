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

#include <stdlib.h>
#include <string.h>

#include "neu_errcodes.h"
#include "neu_types.h"

/*****************/
/* Decimal Types */
/*****************/

neu_double_t neu_bytedec_toDouble(neu_bytedec_t b)
{
    neu_word_t dec = 1;
    for (int m = 0; m < b.decimal; m++)
        dec *= 10;
    return (((neu_double_t)(b.value % dec)) / dec + b.value / dec);
}

neu_double_t neu_ubytedec_toDouble(neu_ubytedec_t ub)
{
    neu_word_t dec = 1;
    for (int m = 0; m < ub.decimal; m++)
        dec *= 10;
    return (((neu_double_t)(ub.value % dec)) / dec + ub.value / dec);
}

neu_double_t neu_worddec_toDouble(neu_worddec_t w)
{
    neu_word_t dec = 1;
    for (int m = 0; m < w.decimal; m++)
        dec *= 10;
    return (((neu_double_t)(w.value % dec)) / dec + w.value / dec);
}

neu_double_t neu_uworddec_toDouble(neu_uworddec_t uw)
{
    neu_word_t dec = 1;
    for (int m = 0; m < uw.decimal; m++)
        dec *= 10;
    return (((neu_double_t)(uw.value % dec)) / dec + uw.value / dec);
}

neu_double_t neu_dworddec_toDouble(neu_dworddec_t dw)
{
    neu_word_t dec = 1;
    for (int m = 0; m < dw.decimal; m++)
        dec *= 10;
    return (((neu_double_t)(dw.value % dec)) / dec + dw.value / dec);
}

neu_double_t neu_udworddec_toDouble(neu_udworddec_t udw)
{
    neu_word_t dec = 1;
    for (int m = 0; m < udw.decimal; m++)
        dec *= 10;
    return (((neu_double_t)(udw.value % dec)) / dec + udw.value / dec);
}

neu_double_t neu_qworddec_toDouble(neu_qworddec_t qw)
{
    neu_word_t dec = 1;
    for (int m = 0; m < qw.decimal; m++)
        dec *= 10;
    return (((neu_double_t)(qw.value % dec)) / dec + qw.value / dec);
}

neu_double_t neu_uqworddec_toDouble(neu_uqworddec_t uqw)
{
    neu_word_t dec = 1;
    for (int m = 0; m < uqw.decimal; m++)
        dec *= 10;
    return (((neu_double_t)(uqw.value % dec)) / dec + uqw.value / dec);
}

/****************/
/* String Types */
/****************/

neu_string_t neu_string_fromArray(char *array)
{
    neu_string_t s;
    s.length  = 0;
    s.charstr = NULL;
    if (!array)
        return s;
    s.length  = strlen(array);
    s.charstr = (char *) array;
    return s;
}

neu_string_t neu_string_fromChars(const char *src)
{
    neu_string_t s;
    s.length  = 0;
    s.charstr = NULL;
    if (!src)
        return s;
    s.length = strlen(src);
    if (s.length > 0) {
        s.charstr = (char *) malloc(s.length);
        if (!s.charstr) {
            s.length = 0;
            return s;
        }
        memcpy(s.charstr, src, s.length);
    } else {
        s.charstr = NULL;
    }
    return s;
}

neu_order_t neu_string_isEqual(const neu_string_t *s1, const neu_string_t *s2)
{
    if (s1->length != s2->length)
        return false;
    if (s1->length == 0)
        return true;
    neu_dword_t is = memcmp((char const *) s1->charstr,
                            (char const *) s2->charstr, s1->length);
    return (is == 0) ? NEU_ORDER_EQ
                     : ((is > 0) ? NEU_ORDER_MORE : NEU_ORDER_LESS);
}

const neu_string_t NEU_STRING_NULL = { 0, NULL };

/**************/
/* Time Types */
/**************/

neu_time_t neu_time_now(void)
{
    return ((neu_time_t) time(0));
}

neu_datetime_t neu_time_getDateTime(neu_time_t t)
{
    uint32_t       days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    neu_datetime_t dt     = { 0 };
    neu_udword_t   pass4year;
    neu_udword_t   yearhours;

    dt.tm_sec = (int) (t % 60);
    t /= 60;
    dt.tm_min = (int) (t % 60);
    t /= 60;
    pass4year = t / (1461L * 24L);

    dt.tm_year = (pass4year << 2) + 1970;
    t %= 1461L * 24L;

    while (1) {
        yearhours = 365 * 24;
        if ((dt.tm_year & 3) == 0) {
            yearhours += 24;
        }
        if (t < yearhours)
            break;
        dt.tm_year++;
        t -= yearhours;
    }

    dt.tm_hour = (int) (t % 24);
    t /= 24;
    t++;

    if ((dt.tm_year & 3) == 0) {
        if (t > 60) {
            t--;
        } else {
            if (t == 60) {
                dt.tm_mon  = 1;
                dt.tm_mday = 29;
                return dt;
            }
        }
    }
    for (dt.tm_mon = 0; days[dt.tm_mon] < t; dt.tm_mon++) {
        t -= days[dt.tm_mon];
    }
    dt.tm_mday = (int) (t);
    return dt;
}

neu_time_t neu_time_fromDateTime(neu_datetime_t dt)
{
    const neu_udword_t mon_yday[2][12] = {
        { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 },
        { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335 },
    };
    neu_word_t   leap = ((dt.tm_year) % 4 == 0 &&
                       ((dt.tm_year) % 100 != 0 || (dt.tm_year) % 400 == 0));
    neu_udword_t ret  = (dt.tm_year - 1970) * 365 * 24 * 3600;
    ret += (mon_yday[leap][dt.tm_mon] + dt.tm_mday - 1) * 24 * 3600;
    ret += dt.tm_hour * 3600 + dt.tm_min * 60 + dt.tm_sec;

    for (int i = 1970; i < dt.tm_year; i++) {
        if (i % 4 == 0 && (i % 100 != 0 || i % 400 == 0)) {
            ret += 24 * 3600;
        }
    }
    if (ret > 4107715199) { // 2100-02-29 23:59:59
        ret += 24 * 3600;
    }
    return (ret);
}

/**************/
/* Uuid Types */
/**************/

neu_boolean_t neu_uuid_isEqual(const neu_uuid_t *u1, const neu_uuid_t *u2)
{
    if (memcmp(u1, u2, sizeof(neu_uuid_t)) == 0)
        return true;
    return false;
}

const neu_uuid_t NEU_UUID_NULL = { 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } };

/****************/
/* NodeId Types */
/****************/

neu_boolean_t neu_nodeid_isNull(const neu_nodeid_t *n)
{
    if (neu_string_isEqual(&n->identifier, &NEU_STRING_NULL) == 0)
        return true;
    return false;
}

/* Ordering for NodeIds */
neu_order_t neu_nodeid_order(const neu_nodeid_t *n1, const neu_nodeid_t *n2)
{
    return (neu_string_isEqual(&n1->identifier, &n2->identifier));
}

const neu_nodeid_t NEU_NODEID_NULL = { { { 0, NULL }, { 0, NULL } },
                                       { 0, NULL } };

/******************/
/* Variable Types */
/******************/

void neu_variable_setScalar(neu_variable_t *v, void *p,
                            const neu_variabletype_t *type)
{
    // init(v) to memset 0;
    v->type        = type;
    v->arrayLength = 0;
    v->data        = p;
}

void neu_variable_setArray(neu_variable_t *v, void *array, size_t arraySize,
                           const neu_variabletype_t *type)
{
    // init(v) to memset 0;
    v->data        = array;
    v->arrayLength = arraySize;
    v->type        = type;
}

neu_datatype_t neu_variable_get_type(neu_variable_t *v)
{
    return NULL == v ? NEU_DATATYPE_ERROR : v->v_type;
}

neu_variable_t *neu_variable_create()
{
    neu_variable_t *v = (neu_variable_t *) malloc(sizeof(neu_variable_t));
    if (NULL == v) {
        return NULL;
    }

    memset(v, 0, sizeof(neu_variable_t));
    return v;
}

void neu_variable_set_error(neu_variable_t *v, const int code)
{
    if (NULL == v) {
        return;
    }
    v->error = code;
}

int neu_variable_get_error(neu_variable_t *v)
{
    if (NULL == v) {
        return -1;
    }
    return v->error;
}

static int neu_variable_set_value(neu_variable_t *v, neu_datatype_t v_type,
                                  void *data, size_t len)
{
    if (NULL == v) {
        return -1;
    }
    if (NULL == data || 0 == len) {
        return -2;
    }

    v->v_type = v_type;
    v->data   = malloc(len);
    if (NULL == v->data) {
        return -3;
    }

    memset(v->data, 0, len);
    memcpy(v->data, data, len);
    v->data_len = len;
    return 0;
}

int neu_variable_set_byte(neu_variable_t *v, const int8_t value)
{
    return neu_variable_set_value(v, NEU_DATATYPE_BYTE, (void *) &value,
                                  sizeof(int8_t));
}

int neu_variable_get_byte(neu_variable_t *v, int8_t *value)
{
    if (NULL == v) {
        return -1;
    }
    *value = *((int8_t *) (v->data));
    return 0;
}

int neu_variable_set_ubyte(neu_variable_t *v, const uint8_t value)
{
    return neu_variable_set_value(v, NEU_DATATYPE_UBYTE, (void *) &value,
                                  sizeof(uint8_t));
}

int neu_variable_set_boolean(neu_variable_t *v, const bool value)
{
    return neu_variable_set_value(v, NEU_DATATYPE_BOOLEAN, (void *) &value,
                                  sizeof(bool));
}

int neu_variable_set_word(neu_variable_t *v, const int16_t value)
{
    return neu_variable_set_value(v, NEU_DATATYPE_WORD, (void *) &value,
                                  sizeof(int16_t));
}

int neu_variable_set_uword(neu_variable_t *v, const uint16_t value)
{
    return neu_variable_set_value(v, NEU_DATATYPE_UWORD, (void *) &value,
                                  sizeof(uint16_t));
}

int neu_variable_set_qword(neu_variable_t *v, const int64_t value)
{
    return neu_variable_set_value(v, NEU_DATATYPE_QWORD, (void *) &value,
                                  sizeof(int64_t));
}

int neu_variable_get_qword(neu_variable_t *v, int64_t *value)
{
    if (NULL == v) {
        return -1;
    }
    *value = *((int64_t *) (v->data));
    return 0;
}

int neu_variable_set_double(neu_variable_t *v, const double value)
{
    return neu_variable_set_value(v, NEU_DATATYPE_DOUBLE, (void *) &value,
                                  sizeof(double));
}

int neu_variable_get_double(neu_variable_t *v, double *value)
{
    if (NULL == v) {
        return -1;
    }
    *value = *((double *) (v->data));
    return 0;
}

int neu_variable_set_string(neu_variable_t *v, const char *value)
{
    return neu_variable_set_value(v, NEU_DATATYPE_STRING, (void *) value,
                                  strlen(value) + 1);
}

int neu_variable_get_string(neu_variable_t *v, char **value, size_t *len)
{
    if (NULL == v) {
        return -1;
    }
    *value = (char *) v->data;
    *len   = v->data_len;
    return 0;
}

int neu_variable_set_bytes(neu_variable_t *v, const unsigned char *value,
                           size_t len)
{
    return neu_variable_set_value(v, NEU_DATATYPE_BYTES, (void *) value, len);
}

void neu_variable_destroy(neu_variable_t *v)
{
    if (NULL == v) {
        return;
    }

    if (NULL != v->next) {
        neu_variable_destroy(v->next);
    }

    if (NULL != v->data) {
        free(v->data);
    }
    free(v);
    return;
}

int neu_variable_add_item(neu_variable_t *v_array, neu_variable_t *v_item)
{
    if (NULL == v_array || NULL == v_item) {
        return -1;
    }

    neu_variable_t *cursor = NULL;
    for (cursor = v_array; NULL != cursor->next; cursor = cursor->next) { }

    cursor->next = v_item;
    v_item->prev = cursor;
    return 0;
}

int neu_variable_get_item(neu_variable_t *v_array, const int index,
                          neu_variable_t **v_item)
{
    int             i = 0;
    neu_variable_t *cursor;
    for (cursor = v_array; NULL != cursor; cursor = cursor->next) {
        if (i == index) {
            *v_item = cursor;
            return 0;
        }
        i++;
    }
    return -1;
}

size_t neu_variable_count(neu_variable_t *v_array)
{
    if (NULL == v_array) {
        return 0;
    }

    neu_variable_t *cursor = NULL;
    size_t          index  = 0;
    for (cursor = v_array; NULL != cursor; cursor = cursor->next) {
        index++;
    }

    return index;
}

static int serialize_join(size_t *size, void *thing, const size_t size_thing,
                          void **data)
{
    if (0 == *size) {
        *data = malloc(size_thing);
        if (NULL == data) {
            return -1; // panic
        }
    }

    if (0 != *size) {
        *data = realloc(*data, *size + size_thing);
        if (NULL == data) {
            return -2; // panic
        }
    }

    memcpy(*data + *size, thing, size_thing);
    *size += size_thing;
    return 0;
}

static size_t neu_variable_serialize_inner(neu_variable_t *v, void **buf,
                                           size_t *len)
{
    if (v == NULL) {
        return 0;
    }

    uint32_t object_len = 0;
    size_t   size_thing = sizeof(uint32_t);
    serialize_join(len, &object_len, size_thing, buf);

    size_thing = sizeof(v->error);
    serialize_join(len, &v->error, size_thing, buf);

    size_thing = sizeof(v->v_type);
    serialize_join(len, &v->v_type, size_thing, buf);

    size_t s_size_thing = sizeof(uint32_t);
    serialize_join(len, &v->data_len, s_size_thing, buf);

    serialize_join(len, v->data, v->data_len, buf);

    uint32_t l = *len;
    memcpy(*buf, &l, sizeof(uint32_t));
    return *len;
}

size_t neu_variable_serialize(neu_variable_t *v, void **buf)
{
    neu_variable_t *cursor  = NULL;
    size_t          len     = 0;
    void *          ser_buf = NULL;
    for (cursor = v; NULL != cursor; cursor = cursor->next) {
        neu_variable_serialize_inner(cursor, &ser_buf, &len);
    }

    *buf = ser_buf;
    return len;
}

void neu_variable_serialize_free(void *buf)
{
    if (NULL != buf) {
        free(buf);
    }
}

static size_t deserialize_join(size_t *size, const void *data, void *thing,
                               size_t size_thing)
{
    memcpy(thing, data + *size, size_thing);
    *size += size_thing;

    return *size;
}

static neu_variable_t *neu_variable_deserialize_inner(void *buf, size_t *len)
{
    neu_variable_t *v = (neu_variable_t *) malloc(sizeof(neu_variable_t));
    if (NULL == v) {
        return NULL;
    }
    memset(v, 0, sizeof(neu_variable_t));

    size_t   size_thing = sizeof(uint32_t);
    uint32_t object_len = 0;
    deserialize_join(len, buf, &object_len, size_thing);

    size_thing = sizeof(v->error);
    deserialize_join(len, buf, &v->error, size_thing);

    size_thing = sizeof(v->v_type);
    deserialize_join(len, buf, &v->v_type, size_thing);

    size_thing = sizeof(uint32_t);
    deserialize_join(len, buf, &v->data_len, size_thing);

    size_thing = v->data_len;
    v->data    = malloc(v->data_len);
    if (NULL == v->data) {
        return NULL;
    }
    deserialize_join(len, buf, v->data, size_thing);

    return v;
}

neu_variable_t *neu_variable_deserialize(void *buf, size_t buf_len)
{
    if (NULL == buf) {
        return NULL;
    }

    if (16 > buf_len) {
        return NULL;
    }

    neu_variable_t *head = NULL;
    size_t          len  = 0;
    head                 = neu_variable_deserialize_inner(buf, &len);
    if (NULL == head) {
        return NULL;
    }

    while (0 != (buf_len - len)) {
        neu_variable_t *v = neu_variable_deserialize_inner(buf, &len);
        neu_variable_add_item(head, v);
    }
    return head;
}

const char *neu_variable_get_str(neu_variable_t *v)
{
    const char *str_buf;

    str_buf = NULL;
    if (v == NULL) {
        return NULL;
    }

    if (v->var_type.typeId == NEU_DATATYPE_STRING) {
        // len = strlen((const char*)v->data);
        str_buf = (const char *) v->data;
    }

    return str_buf;
}
