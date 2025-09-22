#include <nng/nng.h>
#include <nng/supplemental/http/http.h>

#include "define.h"
#include "errcodes.h"
#include "handle.h"
#include "utils/http.h"
#include "utils/log.h"
#include "utils/tpy.h"
#include "json/neu_json_error.h"
#include "json/neu_json_fn.h"

#include "parser/neu_json_cid.h"

void handle_tpy(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();
    (void) plugin;

    NEU_PROCESS_HTTP_REQUEST(
        aio, neu_json_upload_cid_t, neu_json_decode_upload_cid_req, {
            tpy_t tpy      = { 0 };
            int   parse_re = neu_tpy_parse(req->path, &tpy);
            if (parse_re == 0) {
                neu_reqresp_head_t header = { 0 };
                neu_req_add_gtag_t cmd    = { 0 };
                header.ctx                = aio;
                header.type               = NEU_REQ_ADD_GTAG;
                header.otel_trace_type    = NEU_OTEL_TRACE_TYPE_REST_COMM;

                neu_tpy_to_msg(req->driver, &tpy, &cmd);
                int ret = neu_plugin_op(plugin, header, &cmd);
                if (ret != 0) {
                    NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                        neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
                    });
                }
                neu_tpy_free(&tpy);
            } else {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_INVALID_CID, {
                    neu_http_response(aio, NEU_ERR_INVALID_CID, result_error);
                });
            }
        })
}