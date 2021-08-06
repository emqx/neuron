/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2021 EMQ Technologies Co., Ltd All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 **/

#define INPROC_URL "inproc://web_server"

#include <nng/nng.h>
#include <nng/protocol/pair1/pair.h>
#include <nng/protocol/reqrep0/rep.h>
#include <nng/protocol/reqrep0/req.h>
#include <nng/supplemental/http/http.h>
#include <nng/supplemental/util/platform.h>

#include <ctype.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "adapter_internal.h"
#include "neu_log.h"
#include "neu_panic.h"
#include "neu_plugin.h"
#include "neu_webserver.h"
#include "utils/common.h"

// utility function

#define fatal(msg, rv)                             \
    {                                              \
        neu_panic("%s:%s", msg, nng_strerror(rv)); \
    }

typedef enum {
    SEND_REQ, // Sending REQ request
    RECV_REP, // Receiving REQ reply
} job_state;

typedef struct http_msg {
    size_t   method_len;
    char *   method;
    size_t   token_len;
    char *   token;
    size_t   data_len;
    uint8_t *data;
} http_msg;

typedef struct rest_job {
    nng_aio *        http_aio; // aio from HTTP we must reply to
    nng_http_res *   http_res; // HTTP response object
    job_state        state;    // 0 = sending, 1 = receiving
    nng_msg *        msg;      // request message
    nng_aio *        aio;      // request flow
    nng_ctx          ctx;      // context on the request socket
    struct rest_job *next;     // next on the freelist
} rest_job;

nng_socket req_sock;

// We maintain a queue of free jobs.  This way we don't have to
// deallocate them from the callback; we just reuse them.
nng_mtx * job_lock;
rest_job *job_freelist;

// nng_thread *inproc_thr;
// nng_mtx *   mtx_log;
// FILE *      weblog;

static void rest_job_cb(void *arg);

static void rest_recycle_job(rest_job *job)
{
    if (job->http_res != NULL) {
        nng_http_res_free(job->http_res);
        job->http_res = NULL;
    }
    if (job->msg != NULL) {
        nng_msg_free(job->msg);
        job->msg = NULL;
    }
    if (nng_ctx_id(job->ctx) != 0) {
        nng_ctx_close(job->ctx);
    }

    nng_mtx_lock(job_lock);
    job->next    = job_freelist;
    job_freelist = job;
    nng_mtx_unlock(job_lock);
}

static rest_job *rest_get_job(void)
{
    rest_job *job;

    nng_mtx_lock(job_lock);
    if ((job = job_freelist) != NULL) {
        job_freelist = job->next;
        nng_mtx_unlock(job_lock);
        job->next = NULL;
        return (job);
    }
    nng_mtx_unlock(job_lock);
    if ((job = calloc(1, sizeof(*job))) == NULL) {
        return (NULL);
    }
    if (nng_aio_alloc(&job->aio, rest_job_cb, job) != 0) {
        free(job);
        return (NULL);
    }
    return (job);
}

static void rest_http_fatal(rest_job *job, const char *fmt, int rv)
{
    char          buf[128];
    nng_aio *     aio = job->http_aio;
    nng_http_res *res = job->http_res;

    job->http_res = NULL;
    job->http_aio = NULL;
    snprintf(buf, sizeof(buf), fmt, nng_strerror(rv));
    log_fatal("%s", buf);
    nng_http_res_set_status(res, NNG_HTTP_STATUS_INTERNAL_SERVER_ERROR);
    nng_http_res_set_reason(res, buf);
    nng_aio_set_output(aio, 0, res);
    nng_aio_finish(aio, 0);
    rest_recycle_job(job);
}

static void rest_job_cb(void *arg)
{
    rest_job *job = arg;
    nng_aio * aio = job->aio;
    int       rv;

    switch (job->state) {
    case SEND_REQ:
        if ((rv = nng_aio_result(aio)) != 0) {
            rest_http_fatal(job, "send REQ failed: %s", rv);
            return;
        }

        job->msg = NULL;
        // Message was sent, so now wait for the reply.
        nng_aio_set_msg(aio, NULL);
        job->state = RECV_REP;
        nng_ctx_recv(job->ctx, aio);
        break;
    case RECV_REP:
        if ((rv = nng_aio_result(aio)) != 0) {
            rest_http_fatal(job, "recv reply failed: %s", rv);
            return;
        }
        job->msg = nng_aio_get_msg(aio);

        // We got a reply, so give it back to the server.
        rv = nng_http_res_copy_data(job->http_res, nng_msg_body(job->msg),
                                    nng_msg_len(job->msg));
        if (rv != 0) {
            rest_http_fatal(job, "nng_http_res_copy_data: %s", rv);
            return;
        }
        // Set the output - the HTTP server will send it back to the
        // user agent with a 200 response.
        nng_aio_set_output(job->http_aio, 0, job->http_res);
        nng_aio_finish(job->http_aio, 0);
        job->http_aio = NULL;
        job->http_res = NULL;
        // We are done with the job.
        rest_recycle_job(job);
        return;
    default:
        fatal("bad case", NNG_ESTATE);
        break;
    }
}

// Our rest server just takes the message body, creates a request ID
// for it, and sends it on.  This runs in raw mode, so
void rest_handle(nng_aio *aio)
{
    struct rest_job *job;
    nng_http_req *   req = nng_aio_get_input(aio, 0);
    size_t           sz;
    int              rv;
    void *           data;

    if ((job = rest_get_job()) == NULL) {
        nng_aio_finish(aio, NNG_ENOMEM);
        return;
    }
    if (((rv = nng_http_res_alloc(&job->http_res)) != 0) ||
        ((rv = nng_ctx_open(&job->ctx, req_sock)) != 0)) {
        rest_recycle_job(job);
        nng_aio_finish(aio, rv);
        return;
    }

    const char *uri    = nng_http_req_get_uri(req);
    const char *method = nng_http_req_get_method(req);
    const char *header = nng_http_req_get_header(req, "Content-Type");
    const char *token  = nng_http_req_get_header(req, "Authorization");
    log_debug(
        "get http uri: %s, method: %s, Content-type: %s, Authorization: %s",
        uri, method, header, token);

    nng_http_req_get_data(req, &data, &sz);
    job->http_aio = aio;

    http_msg hmsg = { 0 };

    if (method != NULL) {
        hmsg.method_len = strlen(method);
        hmsg.method     = nng_alloc(hmsg.method_len);
        strncpy(hmsg.method, method, hmsg.method_len);
    }

    if (token != NULL) {
        hmsg.token_len = strlen(token);
        hmsg.token     = nng_alloc(hmsg.token_len);
        strncpy(hmsg.token, token, hmsg.token_len);
    }

    if (data != NULL) {
        hmsg.data_len = sz;
        hmsg.data     = nng_alloc(hmsg.data_len);
        memcpy(hmsg.data, data, hmsg.data_len);
    }

    if ((rv = nng_msg_alloc(&job->msg, sizeof(http_msg))) != 0) {
        rest_http_fatal(job, "nng_msg_alloc: %s", rv);
        return;
    }

    memcpy(nng_msg_body(job->msg), &hmsg, sizeof(http_msg));

    nng_aio_set_msg(job->aio, job->msg);
    job->state = SEND_REQ;
    nng_ctx_send(job->ctx, job->aio);
}

void rest_start(uint16_t port)
{
    nng_http_server * server;
    nng_http_handler *handler;
    nng_http_handler *handler_file;
    char              rest_addr[128];
    nng_url *         url;
    int               rv;

    char source[MAX_PATH_SZ + 5] = { 0 };
    get_self_path(source);
    strcat(source, DASHBOARD_PATH);

    log_debug("source dir: %s", source);

    if ((rv = nng_mtx_alloc(&job_lock)) != 0) {
        fatal("nng_mtx_alloc", rv);
    }
    job_freelist = NULL;

    // Set up some strings, etc.  We use the port number
    // from the argument list.
    snprintf(rest_addr, sizeof(rest_addr), REST_URL, port);
    if ((rv = nng_url_parse(&url, rest_addr)) != 0) {
        fatal("nng_url_parse", rv);
    }

    // Create the REQ socket, and put it in raw mode, connected to
    // the remote REP server (our inproc server in this case).
    if ((rv = nng_req0_open(&req_sock)) != 0) {
        fatal("nng_req0_open", rv);
    }
    if ((rv = nng_dial(req_sock, INPROC_URL, NULL, NNG_FLAG_NONBLOCK)) != 0) {
        fatal("nng_dial(" INPROC_URL ")", rv);
    }

    // Get a suitable HTTP server instance.  This creates one
    // if it doesn't already exist.
    if ((rv = nng_http_server_hold(&server, url)) != 0) {
        fatal("nng_http_server_hold", rv);
    }

    // Allocate the handler - we use a dynamic handler for REST
    // using the function "rest_handle" declared above.
    rv = nng_http_handler_alloc(&handler, url->u_path, rest_handle);
    if (rv != 0) {
        fatal("nng_http_handler_alloc", rv);
    }

    if ((rv = nng_http_handler_set_method(handler, NULL)) != 0) {
        fatal("nng_http_handler_set_method", rv);
    }

    // We want to collect the body, and we (arbitrarily) limit this to
    // 128KB.  The default limit is 1MB.  You can explicitly collect
    // the data yourself with another HTTP read transaction by disabling
    // this, but that's a lot of work, especially if you want to handle
    // chunked transfers.
    if ((rv = nng_http_handler_collect_body(handler, true, 1024 * 128)) != 0) {
        fatal("nng_http_handler_collect_body", rv);
    }

    // Specify local directory for root url "/"
    rv = nng_http_handler_alloc_directory(&handler_file, "", source);
    if (rv != 0) {
        fatal("nng_http_handler_alloc_file", rv);
    }

    if ((rv = nng_http_handler_set_method(handler_file, "GET")) != 0) {
        fatal("nng_http_handler_set_method", rv);
    }

    if ((rv = nng_http_handler_collect_body(handler_file, true, 1024)) != 0) {
        fatal("nng_http_handler_collect_body", rv);
    }

    if ((rv = nng_http_server_add_handler(server, handler_file)) != 0) {
        fatal("nng_http_handler_add_handler", rv);
    }
    if ((rv = nng_http_server_add_handler(server, handler)) != 0) {
        fatal("nng_http_handler_add_handler", rv);
    }

    if ((rv = nng_http_server_start(server)) != 0) {
        fatal("nng_http_server_start", rv);
    }

    nng_url_free(url);
}

//
// web_server - this just is a simple REP server that listens for
// messages, and performs ROT13 on them before sending them.  This
// doesn't have to be in the same process -- it is hear for demonstration
// simplicity only.  (Most likely this would be somewhere else.)  Note
// especially that this uses inproc, so nothing can get to it directly
// from outside the process.
//
void web_server(void *arg)
{
    nng_socket s;
    int        rv;
    nng_msg *  msg;

    (void) arg;

    if (((rv = nng_rep0_open(&s)) != 0) ||
        ((rv = nng_listen(s, INPROC_URL, NULL, 0)) != 0)) {
        fatal("unable to set up inproc", rv);
    }
    // This is simple enough that we don't need concurrency.  Plus it
    // makes for an easier demo.
    for (;;) {

        if ((rv = nng_recvmsg(s, &msg, 0)) != 0) {
            fatal("inproc recvmsg", rv);
        }

        http_msg hmsg;
        memcpy(&hmsg, nng_msg_body(msg), sizeof(http_msg));
        log_debug("method: %.*s", hmsg.method_len, hmsg.method);
        log_debug("token: %.*s", hmsg.token_len, hmsg.token);
        log_debug("data: %.*s", hmsg.data_len, hmsg.data);

        // TODO  logic process area
        if (strstr((const char *) hmsg.data, "func")) {
            nng_msg_clear(msg);
            char *res = "reply 'func'";
            nng_msg_append(msg, res, strlen(res));
            if ((rv = nng_sendmsg(s, msg, 0)) != 0) {
                fatal("inproc sendmsg", rv);
            }
        } else {
            nng_msg_clear(msg);
            char *res = "404";
            nng_msg_append(msg, res, strlen(res));
            if ((rv = nng_sendmsg(s, msg, 0)) != 0) {
                fatal("inproc sendmsg", rv);
            }
        }

        // free http_msg;
        if (hmsg.method_len > 0)
            nng_free(hmsg.method, hmsg.method_len);
        if (hmsg.data_len > 0)
            nng_free(hmsg.data, hmsg.data_len);
        if (hmsg.token_len)
            nng_free(hmsg.token, hmsg.token_len);
    }
}

// static void log_lock(bool lock, void *udata)
// {
//     nng_mtx *LOCK = (nng_mtx *) (udata);
//     if (lock)
//         nng_mtx_lock(LOCK);
//     else
//         nng_mtx_unlock(LOCK);
// }

// typedef enum adapter_state {
//     ADAPTER_STATE_NULL,
//     ADAPTER_STATE_IDLE,
//     ADAPTER_STATE_STARTING,
//     ADAPTER_STATE_RUNNING,
//     ADAPTER_STATE_STOPPING,
// } adapter_state_e;

// struct neu_adapter {
//     uint32_t             id;
//     adapter_type_e       type;
//     nng_mtx *            mtx;
//     adapter_state_e      state;
//     bool                 stop;
//     char *               name;
//     neu_manager_t *      manager;
//     nng_pipe             pipe;
//     nng_socket           sock;
//     nng_thread *         thrd;
//     uint32_t             new_req_id;
//     char *               plugin_lib_name;
//     void *               plugin_lib; // handle of dynamic lib
//     neu_plugin_module_t *plugin_module;
//     neu_plugin_t *       plugin;
//     adapter_callbacks_t  cb_funs;
// };

// static uint32_t adapter_get_req_id(neu_adapter_t *adapter)
// {
//     uint32_t req_id;

//     req_id = adapter->new_req_id++;
//     return req_id;
// }

// static void adapter_loop(void *arg)
// {
//     int            rv;
//     neu_adapter_t *adapter;

//     adapter = (neu_adapter_t *) arg;
//     const char *manager_url;
//     manager_url = neu_manager_get_url(adapter->manager);
//     rv          = nng_dial(adapter->sock, manager_url, NULL, 0);
//     if (rv != 0) {
//         neu_panic("The adapter can't dial to %s", manager_url);
//     }

//     nng_mtx_lock(adapter->mtx);
//     adapter->state = ADAPTER_STATE_IDLE;
//     nng_mtx_unlock(adapter->mtx);

//     const char *adapter_str = "adapter started";
//     nng_msg *   out_msg;
//     size_t      msg_size;
//     msg_size = msg_inplace_data_get_size(strlen(adapter_str) + 1);
//     rv       = nng_msg_alloc(&out_msg, msg_size);
//     if (rv == 0) {
//         message_t *msg_ptr;
//         char *     buf_ptr;
//         msg_ptr = (message_t *) nng_msg_body(out_msg);
//         msg_inplace_data_init(msg_ptr, MSG_EVENT_STATUS_STRING, msg_size);
//         buf_ptr = msg_get_buf_ptr(msg_ptr);
//         memcpy(buf_ptr, adapter_str, strlen(adapter_str));
//         buf_ptr[strlen(adapter_str)] = 0;
//         nng_sendmsg(adapter->sock, out_msg, 0);
//     }

//     while (1) {
//         nng_msg *msg;

//         nng_mtx_lock(adapter->mtx);
//         if (adapter->stop) {
//             adapter->state = ADAPTER_STATE_NULL;
//             nng_mtx_unlock(adapter->mtx);
//             log_info("Exit loop of the adapter(%s)", adapter->name);
//             break;
//         }
//         nng_mtx_unlock(adapter->mtx);

//         rv = nng_recvmsg(adapter->sock, &msg, 0);
//         if (rv != 0) {
//             log_warn("Manage pipe no message received");
//             continue;
//         }

//         message_t *pay_msg;
//         pay_msg = nng_msg_body(msg);
//         switch (msg_get_type(pay_msg)) {
//         case MSG_CONFIG_INFO_STRING: {
//             char *buf_ptr;
//             buf_ptr = msg_get_buf_ptr(pay_msg);
//             log_info("Received string: %s", buf_ptr);
//             break;
//         }

//         case MSG_CMD_START_READ: {
//             const neu_plugin_intf_funs_t *intf_funs;
//             neu_request_t                 req;
//             uint32_t                      req_code;
//             intf_funs    = adapter->plugin_module->intf_funs;
//             req.req_id   = adapter_get_req_id(adapter);
//             req.req_type = NEU_REQRESP_READ;
//             req.buf_len  = sizeof(uint32_t);
//             req_code     = 1;
//             req.buf      = (char *) &req_code;
//             intf_funs->request(adapter->plugin, &req);
//             break;
//         }

//         case MSG_CMD_STOP_READ: {
//             break;
//         }

//         case MSG_CMD_EXIT_LOOP: {
//             uint32_t exit_code;

//             exit_code = *(uint32_t *) msg_get_buf_ptr(pay_msg);
//             log_info("adapter(%s) exit loop by exit_code=%d", adapter->name,
//                 exit_code);
//             nng_mtx_lock(adapter->mtx);
//             adapter->state = ADAPTER_STATE_NULL;
//             adapter->stop  = true;
//             nng_mtx_unlock(adapter->mtx);
//             break;
//         }

//         default:
//             log_warn("Receive a not supported message(type: %d)",
//                 msg_get_type(pay_msg));
//             break;
//         }

//         nng_msg_free(msg);
//     }

//     return;
// }

// static int adapter_response(neu_adapter_t *adapter, neu_response_t *resp)
// {
//     int rv = 0;

//     if (adapter == NULL || resp == NULL) {
//         log_warn("The adapter or response is NULL");
//         return (-1);
//     }

//     log_info("Get response from plugin");
//     switch (resp->resp_type) {
//     case NEU_REQRESP_MOVE_BUF: {
//         core_databuf_t *databuf;
//         databuf = core_databuf_new_with_buf(resp->buf, resp->buf_len);
//         // for debug
//         log_debug("Get respose buf: %s", core_databuf_get_ptr(databuf));

//         nng_msg *msg;
//         size_t   msg_size;
//         msg_size = msg_external_data_get_size();
//         rv       = nng_msg_alloc(&msg, msg_size);
//         if (rv == 0) {
//             message_t *pay_msg;

//             pay_msg = (message_t *) nng_msg_body(msg);
//             msg_external_data_init(pay_msg, MSG_DATA_NEURON_DATABUF,
//             databuf); nng_sendmsg(adapter->sock, msg, 0);
//         }
//         core_databuf_put(databuf);
//         break;
//     }

//     default:
//         break;
//     }
//     return rv;
// }

// static int adapter_event_notify(
//     neu_adapter_t *adapter, neu_event_notify_t *event)
// {
//     int rv = 0;

//     log_info("Get event notify from plugin");
//     return rv;
// }

// static const adapter_callbacks_t callback_funs = { .response =
// adapter_response,
//     .event_notify = adapter_event_notify };

// neu_adapter_t *neu_adapter_create(neu_adapter_info_t *info)
// {
//     neu_adapter_t *adapter;

//     nng_mtx_alloc(&mtx_log);
//     log_set_lock(log_lock, mtx_log);
//     log_set_level(LOG_DEBUG);
//     weblog = fopen("web-server.log", "a");
//     // log_set_quiet(true);
//     log_add_fp(weblog, LOG_DEBUG);

//     adapter = malloc(sizeof(neu_adapter_t));
//     if (adapter == NULL) {
//         log_error("Out of memeory for create adapter");
//         return NULL;
//     }

//     int rv;
//     adapter->state = ADAPTER_STATE_NULL;
//     adapter->stop  = false;
//     if ((rv = nng_mtx_alloc(&adapter->mtx)) != 0) {
//         log_error("Can't allocate mutex for adapter");
//         free(adapter);
//         return NULL;
//     }

//     adapter->id   = info->id;
//     adapter->type = info->type;
//     adapter->name = strdup(info->name);
//     // adapter->plugin_lib_name = strdup(info->plugin_lib_name);
//     adapter->new_req_id = 0;

//     if (adapter->name == NULL /*|| adapter->plugin_lib_name == NULL*/) {
//         if (adapter->name != NULL) {
//             free(adapter->name);
//         }
//         if (adapter->plugin_lib_name != NULL) {
//             free(adapter->plugin_lib_name);
//         }
//         nng_mtx_free(adapter->mtx);
//         free(adapter);
//         log_error("Failed duplicate string for create adapter");
//         return NULL;
//     }

//     /** no need plugin
//         void *               handle;
//         neu_plugin_module_t *plugin_module;
//         handle = load_plugin_library(adapter->plugin_lib_name,
//         &plugin_module); if (handle == NULL) {
//             neu_panic("Can't to load library(%s) for plugin(%s)",
//                 adapter->plugin_lib_name, adapter->name);
//         }

//         adapter->plugin_lib    = handle;
//         adapter->plugin_module = plugin_module;
//         neu_plugin_t *plugin;
//         plugin_module = adapter->plugin_module;
//         plugin        = plugin_module->intf_funs->open(adapter,
//         &callback_funs); if (plugin == NULL) {
//             neu_panic("Can't to open plugin(%s)",
//             plugin_module->module_name);
//         }
//     */

//     rv = nng_pair1_open(&adapter->sock);
//     if (rv != 0) {
//         neu_panic("The adapter(%s) can't open pipe", adapter->name);
//     }

//     // adapter->plugin = plugin;
//     return adapter;
// }

// void neu_adapter_destroy(neu_adapter_t *adapter)
// {
//     if (adapter == NULL) {
//         neu_panic("Destroied adapter is NULL");
//     }

//     nng_close(adapter->sock);
//     // adapter->plugin_module->intf_funs->close(adapter->plugin);
//     // unload_plugin_library(adapter->plugin_lib);
//     if (adapter->name != NULL) {
//         free(adapter->name);
//     }
//     // if (adapter->plugin_lib_name != NULL) {
//     //     free(adapter->plugin_lib_name);
//     // }

//     nng_mtx_free(adapter->mtx);
//     free(adapter);
//     fclose(weblog);
//     nng_mtx_free(mtx_log);
//     return;
// }

// int neu_adapter_start(neu_adapter_t *adapter, neu_manager_t *manager)
// {
//     int rv = 0;
//     uint16_t port = 0;

//     if (manager == NULL) {
//         log_error("Start adapter with NULL manager");
//         return (-1);
//     }

//     // const neu_plugin_intf_funs_t *intf_funs;
//     // intf_funs = adapter->plugin_module->intf_funs;
//     // intf_funs->init(adapter->plugin);

//     adapter->manager = manager;

//     rv = nng_thread_create(&adapter->thrd, adapter_loop, adapter);
//     if (rv != 0) {
//         fatal("Cannot start web adapter", rv);
//     }

//     rv = nng_thread_create(&inproc_thr, web_server, NULL);
//     if (rv != 0) {
//         fatal("Cannot start inproc server", rv);
//     }

//     port = port ? port : DEFAULT_PORT;
//     log_debug(REST_URL, port);
//     rest_start(port);

//     return rv;
// }

// int neu_adapter_stop(neu_adapter_t *adapter, neu_manager_t *manager)
// {
//     int rv = 0;

//     log_info("Stop the adapter(%s)", adapter->name);
//     nng_mtx_lock(adapter->mtx);
//     adapter->stop = true;
//     nng_mtx_unlock(adapter->mtx);
//     nng_thread_destroy(adapter->thrd);
//     nng_thread_destroy(inproc_thr);

//     // const neu_plugin_intf_funs_t *intf_funs;
//     // intf_funs = adapter->plugin_module->intf_funs;
//     // intf_funs->uninit(adapter->plugin);

//     return rv;
// }

// const char *neu_adapter_get_name(neu_adapter_t *adapter)
// {
//     return (const char *) adapter->name;
// }

// neu_manager_t *neu_adapter_get_manager(neu_adapter_t *adapter)
// {
//     return (neu_manager_t *) adapter->manager;
// }

// nng_socket neu_adapter_get_sock(neu_adapter_t *adapter)
// {
//     return adapter->sock;
// }
