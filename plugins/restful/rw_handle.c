#include <pthread.h>
#include <stdlib.h>
#include <sys/queue.h>

#include "plugin.h"
#include "utils/log.h"
#include "json/neu_json_fn.h"
#include "json/neu_json_rw.h"

#include "handle.h"
#include "http.h"

#include "rw_handle.h"

void handle_read(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    REST_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_read_req_t, neu_json_decode_read_req, {
            neu_plugin_send_read_cmd(plugin, req->node, req->group,
                                     (void *) aio);
        })
}

void handle_write(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    REST_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_write_req_t, neu_json_decode_write_req, {
            neu_dvalue_t value = { 0 };
            switch (req->t) {
            case NEU_JSON_INT:
                value.type      = NEU_TYPE_INT64;
                value.value.u64 = req->value.val_int;
                break;
            case NEU_JSON_STR:
                value.type = NEU_TYPE_STRING;
                strcpy(value.value.str, req->value.val_str);
                break;
            case NEU_JSON_DOUBLE:
                value.type      = NEU_TYPE_DOUBLE;
                value.value.d64 = req->value.val_double;
                break;
            case NEU_JSON_BOOL:
                value.type          = NEU_TYPE_BOOL;
                value.value.boolean = req->value.val_bool;
                break;
            default:
                assert(false);
                break;
            }

            neu_plugin_send_write_cmd(plugin, req->node, req->group, req->tag,
                                      value, aio);
        })
}

void handle_read_resp(void *cmd_resp)
{
    neu_request_t *          req     = (neu_request_t *) cmd_resp;
    neu_reqresp_read_resp_t *resp    = (neu_reqresp_read_resp_t *) req->buf;
    neu_json_read_resp_t     api_res = { 0 };
    char *                   result  = NULL;

    api_res.n_tag = resp->n_data;
    api_res.tags  = calloc(api_res.n_tag, sizeof(neu_json_read_resp_tag_t));

    for (int i = 0; i < resp->n_data; i++) {
        api_res.tags[i].name  = resp->datas[i].tag;
        api_res.tags[i].error = NEU_ERR_SUCCESS;

        switch (resp->datas[i].value.type) {
        case NEU_TYPE_INT8:
        case NEU_TYPE_UINT8:
            api_res.tags[i].t             = NEU_JSON_INT;
            api_res.tags[i].value.val_int = resp->datas[i].value.value.u8;
            break;
        case NEU_TYPE_INT16:
        case NEU_TYPE_UINT16:
            api_res.tags[i].t             = NEU_JSON_INT;
            api_res.tags[i].value.val_int = resp->datas[i].value.value.u16;
            break;
        case NEU_TYPE_INT32:
        case NEU_TYPE_UINT32:
            api_res.tags[i].t             = NEU_JSON_INT;
            api_res.tags[i].value.val_int = resp->datas[i].value.value.u32;
            break;
        case NEU_TYPE_INT64:
        case NEU_TYPE_UINT64:
            api_res.tags[i].t             = NEU_JSON_INT;
            api_res.tags[i].value.val_int = resp->datas[i].value.value.u64;
            break;
        case NEU_TYPE_FLOAT:
            api_res.tags[i].t               = NEU_JSON_FLOAT;
            api_res.tags[i].value.val_float = resp->datas[i].value.value.f32;
            break;
        case NEU_TYPE_DOUBLE:
            api_res.tags[i].t                = NEU_JSON_DOUBLE;
            api_res.tags[i].value.val_double = resp->datas[i].value.value.d64;
            break;
        case NEU_TYPE_BIT:
            api_res.tags[i].t             = NEU_JSON_BIT;
            api_res.tags[i].value.val_bit = resp->datas[i].value.value.u8;
            break;
        case NEU_TYPE_BOOL:
            api_res.tags[i].t              = NEU_JSON_BOOL;
            api_res.tags[i].value.val_bool = resp->datas[i].value.value.boolean;
            break;
        case NEU_TYPE_STRING:
            api_res.tags[i].t             = NEU_JSON_STR;
            api_res.tags[i].value.val_str = resp->datas[i].value.value.str;
            break;
        case NEU_TYPE_ERROR:
            api_res.tags[i].t             = NEU_JSON_INT;
            api_res.tags[i].value.val_int = resp->datas[i].value.value.i32;
            api_res.tags[i].error         = resp->datas[i].value.value.i32;
            break;
        default:
            break;
        }
    }

    neu_json_encode_by_fn(&api_res, neu_json_encode_read_resp, &result);
    http_ok(resp->ctx, result);
    free(api_res.tags);
    free(result);
}

void handle_write_resp(void *cmd_resp)
{
    neu_request_t *           req  = (neu_request_t *) cmd_resp;
    neu_reqresp_write_resp_t *resp = (neu_reqresp_write_resp_t *) req->buf;

    NEU_JSON_RESPONSE_ERROR(resp->error, {
        http_response(resp->ctx, error_code.error, result_error);
    })
}