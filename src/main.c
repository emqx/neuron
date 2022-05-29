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
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#include <nng/nng.h>
#include <nng/protocol/pair1/pair.h>
#include <nng/supplemental/util/platform.h>

#include "argparse.h"
#include "core/message.h"
#include "core/neu_manager.h"
#include "daemon.h"
#include "restful/rest.h"
#include "utils/idhash.h"
#include "utils/log.h"

static neu_manager_t *g_manager = NULL;
static nng_socket     g_sock    = { 0 };
zlog_category_t *     neuron    = NULL;

static void sig_handler(int sig)
{
    // Receive abort signal, exit the neuron
    nlog_warn("recv sig: %d", sig);

    int      rv;
    size_t   msg_size;
    nng_msg *msg;
    msg_size = msg_inplace_data_get_size(sizeof(uint32_t));
    rv       = nng_msg_alloc(&msg, msg_size);
    assert(rv == 0);

    void *     buf_ptr;
    message_t *pay_msg;
    pay_msg = (message_t *) nng_msg_body(msg);
    msg_inplace_data_init(pay_msg, MSG_CMD_EXIT_LOOP, sizeof(uint32_t));

    buf_ptr               = msg_get_buf_ptr(pay_msg);
    *(uint32_t *) buf_ptr = 0; // exit_code is 0

    if (g_sock.id != 0) {
        nng_sendmsg(g_sock, msg, 0);
    } else {
        nng_msg_free(msg);
    }

    if (g_manager != NULL) {
        neu_manager_stop(g_manager);
    }
    zlog_fini();
}

static void bind_main_adapter(nng_pipe p, nng_pipe_ev ev, void *arg)
{
    neu_manager_t *manager;

    (void) ev;

    manager = (neu_manager_t *) arg;
    neu_manager_trigger_running(manager);
    nlog_notice("Bind the main adapter to neuron manager with pipe(%d)", p.id);
    return;
}

static void unbind_main_adapter(nng_pipe p, nng_pipe_ev ev, void *arg)
{
    (void) ev;
    (void) arg;

    nlog_notice("Unbind the main adapter from neuron manager with pipe(%d)",
                p.id);
    return;
}

static int neuron_run(const neu_cli_args_t *args)
{
    int           rv = 0;
    struct rlimit rl;

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGABRT, sig_handler);

    // try to enable core dump
    rl.rlim_cur = rl.rlim_max = RLIM_INFINITY;
    if (setrlimit(RLIMIT_CORE, &rl) < 0) {
        nlog_warn("neuron process failed enable core dump, ignore");
    }

    zlog_notice(neuron, "neuron process, daemon: %d", args->daemonized);
    g_manager = neu_manager_create();
    if (g_manager == NULL) {
        nlog_fatal("neuron process failed to create neuron manager, exit!");
        return -1;
    }
    neu_manager_wait_ready(g_manager);

    nng_duration recv_timeout = 100;
    rv                        = nng_pair1_open(&g_sock);
    assert(rv == 0);

    nng_socket_set_ms(g_sock, NNG_OPT_RECVTIMEO, recv_timeout);
    rv = neu_manager_init_main_adapter(g_manager, bind_main_adapter,
                                       unbind_main_adapter);

    const char *manager_url;
    nng_dialer  dialer = { 0 };
    manager_url        = neu_manager_get_url(g_manager);
    rv                 = nng_dial(g_sock, manager_url, &dialer, 0);
    assert(rv == 0);

    nlog_notice("neuron process wait for exit");
    neu_manager_destroy(g_manager);
    nlog_notice("neuron process close dialer");
    nng_dialer_close(dialer);

    return rv;
}

int main(int argc, char *argv[])
{
    int            rv     = 0;
    int            status = 0;
    int            signum = 0;
    pid_t          pid    = 0;
    neu_cli_args_t args   = { 0 };

    neu_cli_args_init(&args, argc, argv);

    if (args.daemonized) {
        // become a daemon, this should be before calling `init`
        daemonize();
    }
    neuron = zlog_get_category("neuron");

    if (neuron_already_running()) {
        nlog_fatal("neuron process already running, exit.");
        rv = -1;
        goto main_end;
    }

    for (size_t i = 0; i < args.restart; ++i) {
        if ((pid = fork()) < 0) {
            nlog_error("cannot fork neuron daemon");
            goto main_end;
        } else if (pid == 0) { // child
            break;
        }

        // block waiting for child
        if (pid != waitpid(pid, &status, 0)) {
            nlog_error("cannot wait for neuron daemon");
            goto main_end;
        }

        if (WIFEXITED(status)) {
            status = WEXITSTATUS(status);
            nlog_error("detect neuron daemon exit with status:%d", status);
            if (NEU_RESTART_ONFAILURE == args.restart && 0 == status) {
                goto main_end;
            }
        } else if (WIFSIGNALED(status)) {
            signum = WTERMSIG(status);
            zlog_notice(neuron, "detect neuron daemon term with signal:%d",
                        signum);
        }

        // sleep(1);
    }

    rv = neuron_run(&args);

main_end:
    neu_cli_args_fini(&args);
    zlog_fini();
    return rv;
}
