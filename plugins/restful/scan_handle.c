#include <stdlib.h>

#include "plugin.h"
#include "utils/log.h"
#include "json/neu_json_fn.h"
#include "json/neu_json_scan.h"

#include "handle.h"
#include "utils/http.h"

#include "scan_handle.h"

void handle_scan_tags(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_scan_tags_req_t, neu_json_decode_scan_tags_req, {
            neu_reqresp_head_t  header = { 0 };
            neu_req_scan_tags_t cmd    = { 0 };
            int                 err_type;

            nng_http_req *nng_req = nng_aio_get_input(aio, 0);
            nlog_notice("<%p> req %s %s", aio, nng_http_req_get_method(nng_req),
                        nng_http_req_get_uri(nng_req));
            header.ctx  = aio;
            header.type = NEU_REQ_SCAN_TAGS;

            if (NULL != req->node && NEU_NODE_NAME_LEN <= strlen(req->node)) {
                err_type = NEU_ERR_NODE_NAME_TOO_LONG;
                goto error;
            }

            if (NULL != req->id && NEU_TAG_ADDRESS_LEN <= strlen(req->id)) {
                err_type = NEU_ERR_NODE_NAME_TOO_LONG;
                goto error;
            }

            if (NULL != req->ctx && NEU_VALUE_SIZE <= strlen(req->ctx)) {
                err_type = NEU_ERR_NODE_NAME_TOO_LONG;
                goto error;
            }

            if (NULL != req->node) {
                strcpy(cmd.driver, req->node);
            }

            if (NULL != req->id) {
                strcpy(cmd.id, req->id);
            }

            if (NULL != req->ctx) {
                strcpy(cmd.ctx, req->ctx);
            }

            if (0 != neu_plugin_op(plugin, header, &cmd)) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                    neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
                });
            }
            goto success;

        error:
            NEU_JSON_RESPONSE_ERROR(
                err_type, { neu_http_response(aio, err_type, result_error); });

        success:;
        })
}

void handle_scan_tags_resp(nng_aio *aio, neu_resp_scan_tags_t *resp)
{
    char *result = NULL;
    neu_json_encode_by_fn(resp, neu_json_encode_scan_tags_resp, &result);
    neu_http_ok(aio, result);
    free(result);
}
