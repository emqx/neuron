#include <ctype.h>
#include <jansson.h>
#include <stdlib.h>
#include <string.h>

#include "handle.h"
#include "modbus_tcp_simulator.h"
#include "utils/http.h"
#include "utils/log.h"
#include "json/neu_json_fn.h"

static int extract_hold_start_from_addr(const char *addr_str,
                                        uint16_t *  out_idx0)
{
    if (!addr_str || !out_idx0)
        return -1;
    const char *p = strstr(addr_str, "!4");
    if (!p)
        return -1;
    p += 2;
    if (!isdigit((unsigned char) *p))
        return -1;
    long v = strtol(p, NULL, 10);
    if (v <= 0 || v > 65535)
        return -1;
    *out_idx0 = (uint16_t)(v - 1);
    return 0;
}

void handle_simulator_status(nng_aio *aio)
{
    NEU_VALIDATE_JWT(aio);
    neu_modbus_simulator_status_t st = { 0 };
    neu_modbus_simulator_get_status(&st);
    char buf[256] = { 0 };
    snprintf(buf, sizeof(buf),
             "{\"running\":%d,\"port\":%u,\"tag_count\":%d,\"error\":%d}",
             st.running ? 1 : 0, st.port, st.tag_count, st.error);
    neu_http_ok(aio, buf);
}

void handle_simulator_start(nng_aio *aio)
{
    NEU_VALIDATE_JWT(aio);
    int rc = neu_modbus_simulator_start(NULL, 0);

    neu_modbus_simulator_status_t st = { 0 };
    neu_modbus_simulator_get_status(&st);
    if (rc != 0 || (!st.running && st.error != 0)) {
        char buf[64] = { 0 };
        snprintf(buf, sizeof(buf), "{\"error\":%d}", st.error);
        neu_http_ok(aio, buf);
        return;
    }
    neu_http_ok(aio, "{\"error\":0}");
}

void handle_simulator_list_tags(nng_aio *aio)
{
    NEU_VALIDATE_JWT(aio);
    char *out = neu_modbus_simulator_list_tags_json();
    if (!out) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_EINTERNAL, {
            neu_http_response(aio, NEU_ERR_EINTERNAL, result_error);
        });
        return;
    }
    neu_http_ok(aio, out);
    free(out);
}

void handle_simulator_stop(nng_aio *aio)
{
    NEU_VALIDATE_JWT(aio);
    neu_modbus_simulator_stop();
    neu_http_ok(aio, "{\"error\":0}");
}

void handle_simulator_set_config(nng_aio *aio)
{
    NEU_VALIDATE_JWT(aio);
    char * body = NULL;
    size_t sz   = 0;
    if (neu_http_get_body(aio, (void **) &body, &sz) != 0 || !body) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_PARAM_IS_WRONG, {
            neu_http_response(aio, NEU_ERR_PARAM_IS_WRONG, result_error);
        });
        return;
    }

    json_error_t jerr;
    json_t *     root = json_loads(body, 0, &jerr);
    free(body);
    if (!root) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_PARAM_IS_WRONG, {
            neu_http_response(aio, NEU_ERR_PARAM_IS_WRONG, result_error);
        });
        return;
    }
    json_t *tags = json_object_get(root, "tags");
    if (!tags || !json_is_array(tags)) {
        json_decref(root);
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_PARAM_IS_WRONG, {
            neu_http_response(aio, NEU_ERR_PARAM_IS_WRONG, result_error);
        });
        return;
    }

    int count = 0;
    for (size_t i = 0; i < json_array_size(tags); ++i) {
        json_t *obj = json_array_get(tags, i);
        if (!obj || !json_is_object(obj))
            continue;
        json_t *jaddr = json_object_get(obj, "address");
        json_t *jtype = json_object_get(obj, "type");
        if (!jaddr || !json_is_string(jaddr) || !jtype)
            continue;
        const char *addr_str = json_string_value(jaddr);
        uint16_t    idx0     = 0;
        if (extract_hold_start_from_addr(addr_str, &idx0) != 0)
            continue;
        count++;
    }

    char **             names     = NULL;
    char **             addresses = NULL;
    uint16_t *          indices   = NULL;
    neu_sim_tag_type_e *types     = NULL;
    if (count > 0) {
        names     = calloc((size_t) count, sizeof(char *));
        addresses = calloc((size_t) count, sizeof(char *));
        indices   = calloc((size_t) count, sizeof(uint16_t));
        types     = calloc((size_t) count, sizeof(neu_sim_tag_type_e));
        if (!names || !addresses || !indices || !types) {
            free(names);
            free(addresses);
            free(indices);
            free(types);
            json_decref(root);
            NEU_JSON_RESPONSE_ERROR(NEU_ERR_EINTERNAL, {
                neu_http_response(aio, NEU_ERR_EINTERNAL, result_error);
            });
            return;
        }
    }

    int m = 0;
    for (size_t i = 0; i < json_array_size(tags); ++i) {
        json_t *obj = json_array_get(tags, i);
        if (!obj || !json_is_object(obj))
            continue;
        json_t *jname = json_object_get(obj, "name");
        json_t *jaddr = json_object_get(obj, "address");
        json_t *jtype = json_object_get(obj, "type");
        if (!jaddr || !json_is_string(jaddr) || !jtype)
            continue;
        const char *addr_str = json_string_value(jaddr);
        uint16_t    idx0     = 0;
        if (extract_hold_start_from_addr(addr_str, &idx0) != 0)
            continue;
        neu_sim_tag_type_e tp = NEU_SIM_TAG_NONE;
        if (json_is_string(jtype)) {
            const char *ts = json_string_value(jtype);
            if (ts) {
                if (strcmp(ts, "sine") == 0)
                    tp = NEU_SIM_TAG_SINE;
                else if (strcmp(ts, "saw") == 0)
                    tp = NEU_SIM_TAG_SAW;
                else if (strcmp(ts, "square") == 0)
                    tp = NEU_SIM_TAG_SQUARE;
                else if (strcmp(ts, "random") == 0)
                    tp = NEU_SIM_TAG_RANDOM;
            }
        } else if (json_is_integer(jtype)) {
            tp = (neu_sim_tag_type_e) json_integer_value(jtype);
        }
        if (tp == NEU_SIM_TAG_NONE || idx0 >= 1000)
            continue;
        indices[m] = idx0;
        types[m]   = tp;
        names[m]   = jname && json_is_string(jname)
            ? strdup(json_string_value(jname))
            : strdup("");
        addresses[m] = strdup(addr_str);
        m++;
    }
    json_decref(root);

    neu_modbus_simulator_config_tags(
        (const char **) names, (const char **) addresses, indices, types, m);

    for (int i = 0; i < m; ++i) {
        free(names[i]);
        free(addresses[i]);
    }
    free(names);
    free(addresses);
    free(indices);
    free(types);
    neu_http_ok(aio, "{\"error\":0}");
}

void handle_simulator_export(nng_aio *aio)
{
    NEU_VALIDATE_JWT(aio);
    char *drivers = neu_modbus_simulator_export_drivers_json();
    if (!drivers) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_EINTERNAL, {
            neu_http_response(aio, NEU_ERR_EINTERNAL, result_error);
        });
        return;
    }
    neu_http_response_file(
        aio, drivers, strlen(drivers),
        "attachment; filename=\"simulator_modbus_south.json\"");
    free(drivers);
}