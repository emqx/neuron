#include "msg.h"

#include "read_response.h"

static int encode_attrs(json_t *object, const neu_tag_sample_attrs_t *attrs)
{
    const struct {
        const char *name;
        bool        present;
        int64_t     value;
    } fields[] = {
        { "timestamp", attrs->has_timestamp, attrs->timestamp },
        { "quality", attrs->has_quality, attrs->quality },
        { "source_timestamp", attrs->has_source_timestamp,
          attrs->source_timestamp },
        { "server_timestamp", attrs->has_server_timestamp,
          attrs->server_timestamp },
    };

    for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
        if (fields[i].present &&
            json_object_set_new(object, fields[i].name,
                                json_integer(fields[i].value)) != 0) {
            return -1;
        }
    }
    return 0;
}

int neu_http_encode_read_response(void *json_object, void *param)
{
    neu_http_read_response_t *ctx    = param;
    json_t *                  object = json_object;
    if (ctx == NULL || ctx->response == NULL) {
        return -1;
    }

    int count = ctx->paginate
        ? ((neu_json_read_paginate_resp_t *) ctx->response)->n_tag
        : ((neu_json_read_resp_t *) ctx->response)->n_tag;
    unsigned int length = ctx->tags ? utarray_len(ctx->tags) : 0;
    if (!json_is_object(object) || count < 0 ||
        (count > 0 &&
         (ctx->offset > length ||
          (unsigned int) count > length - ctx->offset))) {
        if (ctx->paginate) {
            neu_json_read_paginate_resp_t *response = ctx->response;
            for (int i = 0; response->tags && i < count; i++) {
                free(response->tags[i].datatag.name);
                free(response->tags[i].datatag.address);
                free(response->tags[i].datatag.description);
                free(response->tags[i].datatag.unit);
            }
        }
        return -1;
    }

    int ret = ctx->paginate
        ? neu_json_encode_read_paginate_resp(json_object, ctx->response)
        : neu_json_encode_read_resp(json_object, ctx->response);
    if (ret != 0) {
        return ret;
    }

    json_t *items =
        json_object_get(json_object, ctx->paginate ? "items" : "tags");
    if (!json_is_array(items) || json_array_size(items) != (size_t) count) {
        return -1;
    }
    for (int i = 0; i < count; i++) {
        void *tag = utarray_eltptr(ctx->tags, ctx->offset + (unsigned int) i);
        const neu_tag_sample_attrs_t *attrs = ctx->paginate
            ? &((neu_resp_tag_value_meta_paginate_t *) tag)->value.attrs
            : &((neu_resp_tag_value_meta_t *) tag)->value.attrs;
        if (encode_attrs(json_array_get(items, i), attrs) != 0) {
            return -1;
        }
    }
    return 0;
}
