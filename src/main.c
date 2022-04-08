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

#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <nng/nng.h>
#include <nng/protocol/pair1/pair.h>
#include <nng/supplemental/util/platform.h>

#include "argparse.h"
#include "config.h"
#include "core/message.h"
#include "core/neu_manager.h"
#include "log.h"
#include "panic.h"
#include "restful/rest.h"
#include "utils/idhash.h"

static neu_manager_t *manager   = NULL;
static nng_socket     main_sock = { 0 };

static nng_mtx *log_mtx;
FILE *          g_logfile;

static void log_lock(bool lock, void *udata)
{
    nng_mtx *mutex = (nng_mtx *) (udata);
    if (lock) {
        nng_mtx_lock(mutex);
    } else {
        nng_mtx_unlock(mutex);
    }
}

static void init(const char *log_file)
{
    nng_mtx_alloc(&log_mtx);
    log_set_lock(log_lock, log_mtx);
    log_set_level(NEU_LOG_DEBUG);
    if (0 == strcmp(log_file, NEU_LOG_STDOUT_FNAME)) {
        g_logfile = stdout;
    } else {
        g_logfile = fopen(log_file, "a");
    }
    if (g_logfile == NULL) {
        fprintf(stderr,
                "Failed to open logfile when"
                "initialize neuron main process");
        abort();
    }
    // log_set_quiet(true);
    log_add_fp(g_logfile, NEU_LOG_DEBUG);
}

static void uninit()
{
    fclose(g_logfile);
    nng_mtx_free(log_mtx);
}

static int read_neuron_config(const char *config)
{
    return neu_config_init(config);
}

static void sig_handler(int sig)
{
    // Receive abort signal, exit the neuron
    log_warn("recv sig: %d", sig);

    int      rv;
    size_t   msg_size;
    nng_msg *msg;
    msg_size = msg_inplace_data_get_size(sizeof(uint32_t));
    rv       = nng_msg_alloc(&msg, msg_size);
    if (rv != 0) {
        neu_panic("Can't allocate msg for stop neuron manager");
    }

    void *     buf_ptr;
    message_t *pay_msg;
    pay_msg = (message_t *) nng_msg_body(msg);
    msg_inplace_data_init(pay_msg, MSG_CMD_EXIT_LOOP, sizeof(uint32_t));

    buf_ptr               = msg_get_buf_ptr(pay_msg);
    *(uint32_t *) buf_ptr = 0; // exit_code is 0

    if (main_sock.id != 0) {
        nng_sendmsg(main_sock, msg, 0);
    } else {
        nng_msg_free(msg);
    }

    if (manager != NULL) {
        neu_manager_stop(manager);
    }
}

static void bind_main_adapter(nng_pipe p, nng_pipe_ev ev, void *arg)
{
    neu_manager_t *manager;

    (void) ev;

    manager = (neu_manager_t *) arg;
    neu_manager_trigger_running(manager);
    log_info("Bind the main adapter to neuron manager with pipe(%d)", p);
    return;
}

static void unbind_main_adapter(nng_pipe p, nng_pipe_ev ev, void *arg)
{
    (void) ev;
    (void) arg;

    log_info("Unbind the main adapter from neuron manager with pipe(%d)", p);
    return;
}

int main(int argc, char *argv[])
{
    int            rv   = 0;
    neu_cli_args_t args = { 0 };

    neu_cli_args_init(&args, argc, argv);

    init(args.log_file);

    rv = read_neuron_config(args.conf_file);
    if (rv < 0) {
        log_error("Failed to get neuron configuration.");
        goto main_end;
    }

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGABRT, sig_handler);

    log_info("running neuron main process, daemon: %d", args.daemonized);
    manager = neu_manager_create();
    if (manager == NULL) {
        log_error("Failed to create neuron manager, exit!");
        rv = -1;
        goto main_end;
    }
    neu_manager_wait_ready(manager);

    nng_duration recv_timeout = 100;
    rv                        = nng_pair1_open(&main_sock);
    if (rv != 0) {
        neu_panic("The main process can't open pipe");
    }

    nng_setopt(main_sock, NNG_OPT_RECVTIMEO, &recv_timeout,
               sizeof(recv_timeout));
    rv = neu_manager_init_main_adapter(manager, bind_main_adapter,
                                       unbind_main_adapter);

    const char *manager_url;
    nng_dialer  dialer = { 0 };
    manager_url        = neu_manager_get_url(manager);
    rv                 = nng_dial(main_sock, manager_url, &dialer, 0);
    if (rv != 0) {
        neu_panic("The main process can't dial to %s", manager_url);
    }

    log_info("neuron main process wait for exit");
    neu_manager_destroy(manager);
    log_info("neuron main process close dialer");
    nng_dialer_close(dialer);

main_end:
    uninit();
    neu_cli_args_fini(&args);
    return rv;
}
