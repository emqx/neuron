#include <stdlib.h>

#include "neu_log.h"
#include "neu_plugin.h"
#include "parser/neu_json_fn.h"
#include "parser/neu_json_rw.h"

#include "handle.h"
#include "http.h"

#include "rw_handle.h"

void handle_read(nng_aio *aio)
{
    neu_plugin_t *plugin = neu_rest_get_plugin();

    REST_PROCESS_HTTP_REQUEST(
        aio, neu_parse_read_req_t, neu_parse_decode_read, {
            neu_taggrp_config_t *config = neu_system_find_group_config(
                plugin, req->node_id, req->group_config_name);
            neu_plugin_send_read_cmd(plugin, req->node_id, config);
        })
}

void handle_write(nng_aio *aio)
{
    (void) aio;
}