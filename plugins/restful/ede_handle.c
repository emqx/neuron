#include <nng/nng.h>
#include <nng/supplemental/http/http.h>

#include "define.h"
#include "errcodes.h"
#include "handle.h"
#include "parser/neu_json_cid.h"
#include "utils/ede.h"
#include "utils/http.h"
#include "utils/log.h"
#include "json/neu_json_error.h"
#include "json/neu_json_fn.h"

void handle_ede(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();
    (void) plugin;

    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_upload_cid_t, neu_json_decode_upload_cid_req, {
            neu_reqresp_head_t header = { 0 };
            neu_req_add_gtag_t cmd    = { 0 };

            header.ctx             = aio;
            header.type            = NEU_REQ_ADD_GTAG;
            header.otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM;

            neu_ede_to_msg(req->driver, req->path, &cmd);
            if (cmd.groups == NULL || cmd.n_group == 0 ||
                cmd.groups[0].tags == NULL || cmd.groups[0].n_tag == 0) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_INVALID_CID, {
                    neu_http_response(aio, NEU_ERR_INVALID_CID, result_error);
                });
            }

            int ret = neu_plugin_op(plugin, header, &cmd);
            if (ret != 0) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                    neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
                });
            }
        })
}