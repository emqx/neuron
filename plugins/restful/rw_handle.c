#include <pthread.h>
#include <stdlib.h>
#include <sys/queue.h>

#include "log.h"
#include "plugin.h"
#include "json/neu_json_fn.h"
#include "json/neu_json_rw.h"

#include "handle.h"
#include "http.h"

#include "rw_handle.h"

struct cmd_ctx {
    uint32_t      event_id;
    nng_aio *     aio;
    neu_plugin_t *plugin;
    uint32_t      node_id;

    TAILQ_ENTRY(cmd_ctx) node;
};

TAILQ_HEAD(, cmd_ctx) read_cmd_ctxs;
TAILQ_HEAD(, cmd_ctx) write_cmd_ctxs;
static pthread_mutex_t read_ctx_mtx;
static pthread_mutex_t write_ctx_mtx;

static void read_add_ctx(uint32_t event_id, nng_aio *aio, neu_plugin_t *plugin,
                         neu_node_id_t node_id);
static void write_add_ctx(uint32_t event_id, nng_aio *aio);
static struct cmd_ctx *read_find_ctx(uint32_t event_id);
static struct cmd_ctx *write_find_ctx(uint32_t event_id);

void handle_rw_init()
{
    TAILQ_INIT(&read_cmd_ctxs);
    TAILQ_INIT(&write_cmd_ctxs);

    pthread_mutex_init(&read_ctx_mtx, NULL);
    pthread_mutex_init(&write_ctx_mtx, NULL);
}

void handle_rw_uninit()
{
    struct cmd_ctx *ctx = NULL;

    pthread_mutex_lock(&read_ctx_mtx);
    ctx = TAILQ_FIRST(&read_cmd_ctxs);
    while (ctx != NULL) {
        TAILQ_REMOVE(&read_cmd_ctxs, ctx, node);
        nng_aio_finish(ctx->aio, 0);
        free(ctx);
        ctx = TAILQ_FIRST(&read_cmd_ctxs);
    }
    pthread_mutex_unlock(&read_ctx_mtx);

    pthread_mutex_lock(&write_ctx_mtx);
    ctx = TAILQ_FIRST(&write_cmd_ctxs);
    while (ctx != NULL) {
        TAILQ_REMOVE(&write_cmd_ctxs, ctx, node);
        nng_aio_finish(ctx->aio, 0);
        free(ctx);
        ctx = TAILQ_FIRST(&write_cmd_ctxs);
    }
    pthread_mutex_unlock(&write_ctx_mtx);

    pthread_mutex_destroy(&read_ctx_mtx);
    pthread_mutex_destroy(&write_ctx_mtx);
}

static void read_add_ctx(uint32_t event_id, nng_aio *aio, neu_plugin_t *plugin,
                         neu_node_id_t node_id)
{
    struct cmd_ctx *ctx = calloc(1, sizeof(struct cmd_ctx));

    pthread_mutex_lock(&read_ctx_mtx);

    ctx->aio      = aio;
    ctx->event_id = event_id;
    ctx->node_id  = node_id;
    ctx->plugin   = plugin;

    TAILQ_INSERT_TAIL(&read_cmd_ctxs, ctx, node);

    pthread_mutex_unlock(&read_ctx_mtx);
}

static void write_add_ctx(uint32_t event_id, nng_aio *aio)
{
    struct cmd_ctx *ctx = calloc(1, sizeof(struct cmd_ctx));

    pthread_mutex_lock(&write_ctx_mtx);

    ctx->aio      = aio;
    ctx->event_id = event_id;

    TAILQ_INSERT_TAIL(&write_cmd_ctxs, ctx, node);

    pthread_mutex_unlock(&write_ctx_mtx);
}

static struct cmd_ctx *read_find_ctx(uint32_t event_id)
{
    struct cmd_ctx *ctx = NULL;
    struct cmd_ctx *ret = NULL;

    pthread_mutex_lock(&read_ctx_mtx);
    TAILQ_FOREACH(ctx, &read_cmd_ctxs, node)
    {
        if (ctx->event_id == event_id) {
            TAILQ_REMOVE(&read_cmd_ctxs, ctx, node);
            ret = ctx;
            break;
        }
    }

    pthread_mutex_unlock(&read_ctx_mtx);

    return ret;
}

static struct cmd_ctx *write_find_ctx(uint32_t event_id)
{
    struct cmd_ctx *ctx = NULL;
    struct cmd_ctx *ret = NULL;

    pthread_mutex_lock(&write_ctx_mtx);

    TAILQ_FOREACH(ctx, &write_cmd_ctxs, node)
    {
        if (ctx->event_id == event_id) {
            TAILQ_REMOVE(&write_cmd_ctxs, ctx, node);
            ret = ctx;
            break;
        }
    }

    pthread_mutex_unlock(&write_ctx_mtx);

    return ret;
}

void handle_read(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    REST_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_read_req_t, neu_json_decode_read_req, {
            neu_node_id_t node_id =
                neu_plugin_get_node_id_by_node_name(plugin, req->node_name);
            if (node_id == 0) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_NODE_NOT_EXIST, {
                    http_response(aio, NEU_ERR_NODE_NOT_EXIST, result_error);
                })
                return;
            }

            neu_taggrp_config_t *config =
                neu_system_find_group_config(plugin, node_id, req->group_name);
            if (config == NULL) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_GRP_CONFIG_NOT_EXIST, {
                    http_response(aio, NEU_ERR_GRP_CONFIG_NOT_EXIST,
                                  result_error);
                })
                return;
            }

            uint32_t event_id = neu_plugin_get_event_id(plugin);
            read_add_ctx(event_id, aio, plugin, node_id);
            neu_plugin_send_read_cmd(plugin, event_id, node_id, config);
        })
}

void handle_write(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    REST_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_write_req_t, neu_json_decode_write_req, {
            neu_node_id_t node_id =
                neu_plugin_get_node_id_by_node_name(plugin, req->node_name);
            if (node_id == 0) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_NODE_NOT_EXIST, {
                    http_response(aio, NEU_ERR_NODE_NOT_EXIST, result_error);
                })
                return;
            }
            neu_taggrp_config_t *config =
                neu_system_find_group_config(plugin, node_id, req->group_name);
            if (config == NULL) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_GRP_CONFIG_NOT_EXIST, {
                    http_response(aio, NEU_ERR_GRP_CONFIG_NOT_EXIST,
                                  result_error);
                })
                return;
            }
            neu_datatag_table_t *table =
                neu_system_get_datatags_table(plugin, node_id);

            neu_data_val_t *data = neu_parse_write_req_to_val(req, table);
            if (data == NULL) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_TAG_NOT_EXIST, {
                    http_response(aio, NEU_ERR_TAG_NOT_EXIST, result_error);
                })
                return;
            }
            uint32_t event_id = neu_plugin_get_event_id(plugin);
            write_add_ctx(event_id, aio);
            neu_plugin_send_write_cmd(plugin, event_id, node_id, config, data);
        })
}

void handle_read_resp(void *cmd_resp)
{
    neu_request_t *          req     = (neu_request_t *) cmd_resp;
    struct cmd_ctx *         ctx     = read_find_ctx(req->req_id);
    neu_reqresp_read_resp_t *resp    = (neu_reqresp_read_resp_t *) req->buf;
    neu_fixed_array_t *      array   = NULL;
    neu_json_read_resp_t     api_res = { 0 };
    char *                   result  = NULL;
    neu_int_val_t *          iv      = NULL;
    neu_datatag_table_t *    datatag_table =
        neu_system_get_datatags_table(ctx->plugin, ctx->node_id);

    log_info("read resp id: %d, ctx: %p", req->req_id, ctx);
    assert(ctx != NULL);

    neu_dvalue_get_ref_array(resp->data_val, &array);

    api_res.n_tag = array->length;
    api_res.tags  = calloc(api_res.n_tag, sizeof(neu_json_read_resp_tag_t));

    for (size_t i = 0; i < array->length; i++) {
        iv = (neu_int_val_t *) neu_fixed_array_get(array, i);

        neu_datatag_t *tag =
            neu_datatag_tbl_get(datatag_table, (datatag_id_t) iv->key);

        if (tag == NULL) {
            continue;
        }

        api_res.tags[i].name  = tag->name;
        api_res.tags[i].error = NEU_ERR_SUCCESS;

        switch (neu_dvalue_get_value_type(iv->val)) {
        case NEU_DTYPE_ERRORCODE: {
            int32_t error = NEU_ERR_SUCCESS;

            neu_dvalue_get_errorcode(iv->val, &error);
            api_res.tags[i].t             = NEU_JSON_INT;
            api_res.tags[i].value.val_int = error;
            api_res.tags[i].error         = error;

            break;
        }
        case NEU_DTYPE_BIT: {
            uint8_t bit = 0;

            neu_dvalue_get_bit(iv->val, &bit);
            api_res.tags[i].t             = NEU_JSON_BIT;
            api_res.tags[i].value.val_bit = bit;
            break;
        }
        case NEU_DTYPE_INT8: {
            int8_t i8 = 0;

            neu_dvalue_get_int8(iv->val, &i8);
            api_res.tags[i].t             = NEU_JSON_INT;
            api_res.tags[i].value.val_int = i8;
            break;
        }
        case NEU_DTYPE_UINT8: {
            uint8_t u8 = 0;

            neu_dvalue_get_uint8(iv->val, &u8);
            api_res.tags[i].t             = NEU_JSON_INT;
            api_res.tags[i].value.val_int = u8;
            break;
        }
        case NEU_DTYPE_BOOL: {
            bool b = true;

            neu_dvalue_get_bool(iv->val, &b);
            api_res.tags[i].t              = NEU_JSON_BOOL;
            api_res.tags[i].value.val_bool = b;
            break;
        }
        case NEU_DTYPE_UINT16: {
            uint16_t u16 = 0;

            neu_dvalue_get_uint16(iv->val, &u16);
            api_res.tags[i].t             = NEU_JSON_INT;
            api_res.tags[i].value.val_int = u16;
            break;
        }
        case NEU_DTYPE_INT16: {
            int16_t i16 = 0;

            neu_dvalue_get_int16(iv->val, &i16);
            api_res.tags[i].t             = NEU_JSON_INT;
            api_res.tags[i].value.val_int = i16;
            break;
        }
        case NEU_DTYPE_INT32: {
            int32_t i32 = 0;

            neu_dvalue_get_int32(iv->val, &i32);
            api_res.tags[i].t             = NEU_JSON_INT;
            api_res.tags[i].value.val_int = i32;
            break;
        }
        case NEU_DTYPE_UINT32: {
            uint32_t u32 = 0;

            neu_dvalue_get_uint32(iv->val, &u32);
            api_res.tags[i].t             = NEU_JSON_INT;
            api_res.tags[i].value.val_int = u32;
            break;
        }
        case NEU_DTYPE_INT64: {
            int64_t i64 = 0;

            neu_dvalue_get_int64(iv->val, &i64);
            api_res.tags[i].t             = NEU_JSON_INT;
            api_res.tags[i].value.val_int = i64;
            break;
        }
        case NEU_DTYPE_UINT64: {
            uint64_t u64 = 0;

            neu_dvalue_get_uint64(iv->val, &u64);
            api_res.tags[i].t             = NEU_JSON_INT;
            api_res.tags[i].value.val_int = u64;
            break;
        }
        case NEU_DTYPE_FLOAT: {
            float f32 = 0;

            neu_dvalue_get_float(iv->val, &f32);
            api_res.tags[i].t               = NEU_JSON_FLOAT;
            api_res.tags[i].value.val_float = f32;
            break;
        }
        case NEU_DTYPE_DOUBLE: {
            double d64 = 0;

            neu_dvalue_get_double(iv->val, &d64);
            api_res.tags[i].t                = NEU_JSON_DOUBLE;
            api_res.tags[i].value.val_double = d64;
            break;
        }
        case NEU_DTYPE_CSTR: {
            char *cstr = NULL;

            neu_dvalue_get_ref_cstr(iv->val, &cstr);
            api_res.tags[i].t             = NEU_JSON_STR;
            api_res.tags[i].value.val_str = cstr;
        }
        default:
            break;
        }
    }

    neu_json_encode_by_fn(&api_res, neu_json_encode_read_resp, &result);
    http_ok(ctx->aio, result);
    free(ctx);
    free(api_res.tags);
    free(result);
}

void handle_write_resp(void *cmd_resp)
{
    neu_request_t *           req   = (neu_request_t *) cmd_resp;
    struct cmd_ctx *          ctx   = write_find_ctx(req->req_id);
    neu_reqresp_write_resp_t *resp  = (neu_reqresp_write_resp_t *) req->buf;
    neu_fixed_array_t *       array = NULL;
    neu_int_val_t *           iv    = NULL;
    int32_t                   error = 0;

    log_info("write resp id: %d, ctx: %p", req->req_id, ctx);

    neu_dvalue_get_ref_array(resp->data_val, &array);
    iv = (neu_int_val_t *) neu_fixed_array_get(array, 0);
    assert(neu_dvalue_get_value_type(iv->val) == NEU_DTYPE_ERRORCODE);
    neu_dvalue_get_errorcode(iv->val, &error);

    if (ctx != NULL) {
        NEU_JSON_RESPONSE_ERROR(
            error, { http_response(ctx->aio, error_code.error, result_error); })
        free(ctx);
    }
    assert(ctx != NULL);
}