
#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <jwt.h>
#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "json/neu_json_fn.h"

#include "handle.h"
#include "utils/http.h"
#include "utils/neu_jwt.h"

#include "parser/neu_json_system.h"
#include "system_handle.h"

void handle_system_ctl(nng_aio *aio)
{
    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_system_ctl_req_t, neu_json_decode_system_ctl_req, {
            if (req->action == 1) {
                neu_http_ok(aio, "{\"error\": 0 }");
                exit(-2);
            }
        })
}