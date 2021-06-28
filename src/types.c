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

#include <types.h>
#include <errorcodes.h>

/****************/
/* String Types */
/****************/

np_string_t
np_string_fromArray(char *array) {
    np_string_t s; s.length = 0; s.charstr = NULL;
    if(!array)
        return s;
    s.length = strlen(array); s.charstr = (char*)array; 
    return s;
}

np_string_t
np_string_fromChars(const char *src) {
    np_string_t s; s.length = 0; s.charstr = NULL;
    if(!src)
        return s;
    s.length = strlen(src);
    if(s.length > 0) {
        s.charstr = (np_ubyte_t*)UA_malloc(s.length);
        if(!s.charstr) {
            s.length = 0;
            return s;
        }
        memcpy(s.charstr, src, s.length);
    } else {
        s.charstr = NULL;
    }
    return s;
}

np_order_t
np_string_isEqual(const np_string_t *s1, const np_string_t *s2) {
    if(s1->length != s2->length)
        return false;
    if(s1->length == 0)
        return true;
    np_dword_t is = memcmp((char const*)s1->charstr,
                    (char const*)s2->charstr, s1->length);
    return (is == 0) ? NP_ORDER_EQ : ((is > 0) ? NP_ORDER_MORE : NP_ORDER_LESS);
}

const np_string_t NP_STRING_NULL = {0, NULL};

/**************/
/* Time Types */
/**************/

np_time_t 
np_time_now(void) {
    return ((np_time_t) time(0)); 
}

/**************/
/* Uuid Types */
/**************/

np_boolean_t
np_uuid_isEqual(const np_uuid_t *u1, const np_uuid_t *u2) {
    if(memcmp(u1, u2, sizeof(np_uuid_t)) == 0)
        return true;
    return false;
}

const np_uuid_t NP_UUID_NULL = {0, 0, 0, {0,0,0,0,0,0,0,0}};

/****************/
/* NodeId Types */
/****************/

np_boolean_t
np_nodeid_isNull(const np_nodeid_t *n) {
    if (np_string_isEqual(&n->identifier, &NP_STRING_NULL) == 0)
        return true;
    return false;
}

/* Ordering for NodeIds */
np_order_t
np_nodeid_order(const np_nodeid_t *n1, const np_nodeid_t *n2) {
    return (np_string_isEqual(&n1->identifier, &n2->identifier));
}

const np_nodeid_t NP_NODEID_NULL = {{{0, NULL}, {0, NULL}}, {0, NULL}};

/******************/
/* Variable Types */
/******************/

void
np_variable_setScalar(np_variable_t *v, void *p,
                     const np_variabletype_t *type) {
    // init(v) to memset 0;
    v->type = type;
    v->arrayLength = 0;
    v->data = p;
}

void 
np_variable_setArray(np_variable_t *v, void *array,
                         size_t arraySize, const np_variabletype_t *type) {
    // init(v) to memset 0;
    v->data = array;
    v->arrayLength = arraySize;
    v->type = type;
}


