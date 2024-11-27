#include <nng/nng.h>
#include <nng/supplemental/http/http.h>

#include "define.h"
#include "errcodes.h"
#include "handle.h"
#include "utils/cid.h"
#include "utils/http.h"
#include "utils/log.h"
#include "json/neu_json_error.h"
#include "json/neu_json_fn.h"

#include "parser/neu_json_cid.h"

void handle_cid(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();
    (void) plugin;

    NEU_PROCESS_HTTP_REQUEST(
        aio, neu_json_upload_cid_t, neu_json_decode_upload_cid_req, {
            cid_t cid      = { 0 };
            int   parse_re = neu_cid_parse(req->path, &cid);
            if (parse_re == 0) {
                neu_reqresp_head_t header = { 0 };
                neu_req_add_gtag_t cmd    = { 0 };
                header.ctx                = aio;
                header.type               = NEU_REQ_ADD_GTAG;
                header.otel_trace_type    = NEU_OTEL_TRACE_TYPE_REST_COMM;

                nlog_notice("cid parse success: %s", req->path);
                neu_cid_to_msg(req->driver, &cid, &cmd);
                int ret = neu_plugin_op(plugin, header, &cmd);
                if (ret != 0) {
                    NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                        neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
                    });
                }
                neu_cid_free(&cid);
            } else {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_INVALID_CID, {
                    neu_http_response(aio, NEU_ERR_INVALID_CID, result_error);
                });
            }
        })
}