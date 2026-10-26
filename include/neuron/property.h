#ifndef NEURON_PROPERTY_H
#define NEURON_PROPERTY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include <jansson.h>

#ifdef __cplusplus
extern "C" {
#endif
char *neu_node_property_json_dump(const json_t *value);
#ifdef __cplusplus
}
#endif

typedef struct {
    char *key;
    char *value_json;
} neu_node_property_t;

static inline void neu_node_property_fini(neu_node_property_t *property)
{
    free(property->key);
    free(property->value_json);
    property->key        = NULL;
    property->value_json = NULL;
}

static inline void neu_node_properties_free(neu_node_property_t *properties,
                                            size_t               n_property)
{
    for (size_t i = 0; i < n_property; ++i) {
        neu_node_property_fini(&properties[i]);
    }
    free(properties);
}

static inline int neu_node_property_value_kind(const json_t *value)
{
    if (json_is_string(value)) {
        return 1;
    }
    if (json_is_boolean(value)) {
        return 2;
    }
    if (json_is_number(value)) {
        return 3;
    }
    return 0;
}

static inline bool neu_node_property_value_valid(const json_t *value)
{
    if (neu_node_property_value_kind(value) != 0) {
        return true;
    }
    if (!json_is_array(value)) {
        return false;
    }
    int kind = 0;
    for (size_t i = 0; i < json_array_size(value); ++i) {
        int item_kind = neu_node_property_value_kind(json_array_get(value, i));
        if (item_kind == 0 || (kind != 0 && kind != item_kind)) {
            return false;
        }
        kind = item_kind;
    }
    return true;
}

#endif
