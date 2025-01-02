#include <jansson.h>

#include "schema.h"

int mqtt_schema_validate(const char *schema, mqtt_schema_vt_t **vts,
                         size_t *vts_len)
{
    json_t *root = json_loads(schema, 0, NULL);
    if (root == NULL) {
        return -1;
    }

    *vts_len = 0;
    *vts     = NULL;

    const char *key   = NULL;
    json_t *    value = NULL;

    json_object_foreach(root, key, value)
    {
        if (!json_is_string(value)) {
            json_decref(root);
            if (*vts_len > 0) {
                free(*vts);
            }
            return -1;
        }

        const char *str_val = json_string_value(value);
        if (str_val == NULL) {
            json_decref(root);
            if (*vts_len > 0) {
                free(*vts);
            }
            return -1;
        }

        *vts_len += 1;
        *vts = realloc(*vts, *vts_len * sizeof(mqtt_schema_vt_t));
        mqtt_schema_vt_t *vt = &(*vts)[*vts_len - 1];

        memset(vt, 0, sizeof(mqtt_schema_vt_t));
        strcpy(vt->name, key);

        if (strcmp(str_val, "${timestamp}") == 0) {
            vt->vt = MQTT_SCHEMA_TIMESTAMP;
        } else if (strcmp(str_val, "${node}") == 0) {
            vt->vt = MQTT_SCHEMA_NODE_NAME;
        } else if (strcmp(str_val, "${group}")) {
            vt->vt = MQTT_SCHEMA_GROUP_NAME;
        } else if (strcmp(str_val, "${tag_valeus}")) {
            vt->vt = MQTT_SCHEMA_TAG_VALUES;
        } else if (strcmp(str_val, "${static_tag_values}")) {
            vt->vt = MQTT_SCHEMA_STATIC_TAG_VALUES;
        } else if (strcmp(str_val, "${tag_errors}")) {
            vt->vt = MQTT_SCHEMA_TAG_ERRORS;
        } else {
            if (str_val[0] == '$' && str_val[1] == '{' &&
                str_val[strlen(str_val) - 1] == '}') {
                json_decref(root);
                free(*vts);
                return -1;
            } else {
                vt->vt = MQTT_SCHEMA_UD;
                strcpy(vt->ud, str_val);
            }
        }
    }

    return 0;
}

int mqtt_schema_encode(char *driver, char *group, neu_json_read_resp_t *tags,
                       mqtt_schema_vt_t *vts, size_t n_vts, char **result_str)
{
    void *root = neu_json_encode_new();

    for (size_t i = 0; i < n_vts; i++) {
        neu_json_elem_t elem = {
            .name = vts[i].name,
        };
        switch (vts[i].vt) {
        case MQTT_SCHEMA_TIMESTAMP:
            elem.t         = NEU_JSON_INT;
            elem.v.val_int = global_timestamp;
            break;
        case MQTT_SCHEMA_NODE_NAME:
            elem.t         = NEU_JSON_STR;
            elem.v.val_str = driver;
            break;
        case MQTT_SCHEMA_GROUP_NAME:
            elem.t         = NEU_JSON_STR;
            elem.v.val_str = group;
            break;
        case MQTT_SCHEMA_TAG_VALUES: {
            void *values_array = neu_json_array();

            neu_json_read_resp_tag_t *p_tag = tags->tags;

            for (int i = 0; i < tags->n_tag; i++) {
                neu_json_elem_t tag_elems[2 + NEU_TAG_META_SIZE] = { 0 };

                if (p_tag->error == 0) {
                    tag_elems[0].name      = "name";
                    tag_elems[0].t         = NEU_JSON_STR;
                    tag_elems[0].v.val_str = p_tag->name;

                    tag_elems[1].name      = "value";
                    tag_elems[1].t         = p_tag->t;
                    tag_elems[1].v         = p_tag->value;
                    tag_elems[1].precision = p_tag->precision;

                    for (int k = 0; k < p_tag->n_meta; k++) {
                        tag_elems[2 + k].name = p_tag->metas[k].name;
                        tag_elems[2 + k].t    = p_tag->metas[k].t;
                        tag_elems[2 + k].v    = p_tag->metas[k].value;
                    }

                    values_array = neu_json_encode_array(
                        values_array, tag_elems, 2 + p_tag->n_meta);
                }

                p_tag++;
            }

            elem.t            = NEU_JSON_OBJECT;
            elem.v.val_object = values_array;
            break;
        }
        case MQTT_SCHEMA_STATIC_TAG_VALUES:
            // todo
            break;
        case MQTT_SCHEMA_TAG_ERRORS: {
            void *errors_array = neu_json_array();

            neu_json_read_resp_tag_t *p_tag = tags->tags;

            for (int i = 0; i < tags->n_tag; i++) {
                neu_json_elem_t tag_elems[2 + NEU_TAG_META_SIZE] = { 0 };

                if (p_tag->error != 0) {
                    tag_elems[0].name      = "name";
                    tag_elems[0].t         = NEU_JSON_STR;
                    tag_elems[0].v.val_str = p_tag->name;

                    tag_elems[1].name      = "error";
                    tag_elems[1].t         = NEU_JSON_INT;
                    tag_elems[1].v.val_int = p_tag->error;

                    errors_array =
                        neu_json_encode_array(errors_array, tag_elems, 2);
                }

                p_tag++;
            }

            elem.t            = NEU_JSON_OBJECT;
            elem.v.val_object = errors_array;
            break;
        }
        case MQTT_SCHEMA_UD:
            elem.t         = NEU_JSON_STR;
            elem.v.val_str = vts[i].ud;
            break;
        }

        neu_json_encode_field(root, &elem, 1);
    }

    int ret = neu_json_encode(root, result_str);
    neu_json_decode_free(root);
    return ret;
}

int mqtt_static_validate(const char *static_tags, mqtt_static_vt_t *vts,
                         size_t *vts_len)
{
    json_t *root = json_loads(static_tags, 0, NULL);
    if (root == NULL) {
        return -1;
    }

    *vts_len = 0;

    const char *key   = NULL;
    json_t *    value = NULL;

    json_object_foreach(root, key, value)
    {
        *vts_len += 1;
        vts = realloc(vts, *vts_len * sizeof(mqtt_static_vt_t));
        mqtt_static_vt_t *vt = &vts[*vts_len - 1];

        memset(vt, 0, sizeof(mqtt_static_vt_t));
        strcpy(vt->name, key);

        if (json_is_string(value)) {
            vt->jtype          = NEU_JSON_STR;
            vt->jvalue.val_str = strdup(json_string_value(value));
        } else if (json_is_real(value)) {
            vt->jtype             = NEU_JSON_DOUBLE;
            vt->jvalue.val_double = json_real_value(value);
        } else if (json_is_integer(value)) {
            vt->jtype          = NEU_JSON_INT;
            vt->jvalue.val_int = json_integer_value(value);
        } else if (json_is_boolean(value)) {
            vt->jtype           = NEU_JSON_BOOL;
            vt->jvalue.val_bool = json_boolean_value(value);
        } else {
            vt->jtype          = NEU_JSON_STR;
            vt->jvalue.val_str = json_dumps(value, JSON_ENCODE_ANY);
        }
    }

    return 0;
}

void mqtt_static_free(mqtt_static_vt_t *vts, size_t vts_len)
{
    for (size_t i = 0; i < vts_len; i++) {
        if (vts[i].jtype == NEU_JSON_STR) {
            free(vts[i].jvalue.val_str);
        }
    }

    free(vts);
}