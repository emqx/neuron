#include <locale.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "property.h"

typedef struct {
    char * data;
    size_t length;
    size_t capacity;
} property_json_buffer_t;

static int append(property_json_buffer_t *buffer, const char *text)
{
    size_t length = strlen(text);
    if (length >= SIZE_MAX - buffer->length) {
        return -1;
    }
    size_t needed = buffer->length + length + 1;
    if (needed > buffer->capacity) {
        size_t capacity = buffer->capacity ? buffer->capacity : 64;
        while (capacity < needed) {
            if (capacity > SIZE_MAX / 2) {
                capacity = needed;
                break;
            }
            capacity *= 2;
        }
        char *data = realloc(buffer->data, capacity);
        if (data == NULL) {
            return -1;
        }
        buffer->data     = data;
        buffer->capacity = capacity;
    }
    memcpy(buffer->data + buffer->length, text, length + 1);
    buffer->length += length;
    return 0;
}

static int dump_value(property_json_buffer_t *buffer, const json_t *value)
{
    if (json_is_real(value)) {
        char text[64];
        int  length =
            snprintf(text, sizeof(text), "%.17g", json_real_value(value));
        if (length < 0 || (size_t) length >= sizeof(text) - 2) {
            return -1;
        }
        const char *point   = localeconv()->decimal_point;
        char *      decimal = *point ? strstr(text, point) : NULL;
        if (decimal != NULL && strcmp(point, ".") != 0) {
            memmove(decimal + 1, decimal + strlen(point),
                    strlen(decimal + strlen(point)) + 1);
            *decimal = '.';
        }
        if (strpbrk(text, ".eE") == NULL) {
            strcat(text, ".0");
        }
        return append(buffer, text);
    }
    if (json_is_array(value)) {
        if (append(buffer, "[") != 0) {
            return -1;
        }
        for (size_t i = 0; i < json_array_size(value); ++i) {
            if ((i != 0 && append(buffer, ",") != 0) ||
                dump_value(buffer, json_array_get(value, i)) != 0) {
                return -1;
            }
        }
        return append(buffer, "]");
    }
    if (json_is_object(value)) {
        if (append(buffer, "{") != 0) {
            return -1;
        }
        const char *key;
        json_t *    item;
        bool        first = true;
        json_object_foreach((json_t *) value, key, item)
        {
            json_t *name = json_string(key);
            if (name == NULL) {
                return -1;
            }
            int ret = (!first && append(buffer, ",") != 0) ||
                dump_value(buffer, name) != 0 || append(buffer, ":") != 0 ||
                dump_value(buffer, item) != 0;
            json_decref(name);
            if (ret != 0) {
                return -1;
            }
            first = false;
        }
        return append(buffer, "}");
    }
    char *text = json_dumps(value, JSON_COMPACT | JSON_ENCODE_ANY);
    if (text == NULL) {
        return -1;
    }
    int ret = append(buffer, text);
    free(text);
    return ret;
}

char *neu_node_property_json_dump(const json_t *value)
{
    property_json_buffer_t buffer = { 0 };
    if (dump_value(&buffer, value) != 0) {
        free(buffer.data);
        return NULL;
    }
    return buffer.data;
}
