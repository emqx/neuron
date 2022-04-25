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

#include <zlog.h>

#include <nng/nng.h>
#include <nng/protocol/pair1/pair.h>
#include <nng/supplemental/util/platform.h>

#include "argparse.h"
#include "config.h"
#include "core/message.h"
#include "core/neu_manager.h"
#include "daemon.h"
#include "log.h"
#include "panic.h"
#include "restful/rest.h"
#include "utils/idhash.h"

static neu_manager_t * g_manager = NULL;
static nng_socket      g_sock    = { 0 };
static pthread_mutex_t g_log_mtx = PTHREAD_MUTEX_INITIALIZER;
static FILE *          g_log_file;

static void log_lock(bool lock, void *udata)
{
    pthread_mutex_t *mutex = udata;
    if (lock) {
        pthread_mutex_lock(mutex);
    } else {
        pthread_mutex_unlock(mutex);
    }
}

static void init(const char *log_file)
{
    log_set_lock(log_lock, &g_log_mtx);
    log_set_level(NEU_LOG_DEBUG);
    if (0 == strcmp(log_file, NEU_LOG_STDOUT_FNAME)) {
        g_log_file = stdout;
    } else {
        g_log_file = fopen(log_file, "a");
    }
    if (g_log_file == NULL) {
        fprintf(stderr,
                "Failed to open logfile when"
                "initialize neuron main process");
        abort();
    }
    // log_set_quiet(true);
    log_add_fp(g_log_file, NEU_LOG_DEBUG);
}

static void uninit()
{
    fclose(g_log_file);
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

    if (g_sock.id != 0) {
        nng_sendmsg(g_sock, msg, 0);
    } else {
        nng_msg_free(msg);
    }

    if (g_manager != NULL) {
        neu_manager_stop(g_manager);
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
        log_error("neuron process failed enable core dump, ignore");
    }

    log_info("neuron process, daemon: %d", args->daemonized);
    g_manager = neu_manager_create();
    if (g_manager == NULL) {
        log_error("neuron process failed to create neuron manager, exit!");
        return -1;
    }
    neu_manager_wait_ready(g_manager);

    nng_duration recv_timeout = 100;
    rv                        = nng_pair1_open(&g_sock);
    if (rv != 0) {
        neu_panic("neuron process can't open pipe");
    }

    nng_setopt(g_sock, NNG_OPT_RECVTIMEO, &recv_timeout, sizeof(recv_timeout));
    rv = neu_manager_init_main_adapter(g_manager, bind_main_adapter,
                                       unbind_main_adapter);

    const char *manager_url;
    nng_dialer  dialer = { 0 };
    manager_url        = neu_manager_get_url(g_manager);
    rv                 = nng_dial(g_sock, manager_url, &dialer, 0);
    if (rv != 0) {
        neu_panic("neuron process can't dial to %s", manager_url);
    }

    log_info("neuron process wait for exit");
    neu_manager_destroy(g_manager);
    log_info("neuron process close dialer");
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

    zlog_init("./config/zlog.conf");
    neu_cli_args_init(&args, argc, argv);

    if (args.daemonized) {
        // become a daemon, this should be before calling `init`
        daemonize();
    }

    init(args.log_file);

    if (neuron_already_running()) {
        log_error("neuron process already running, exit.");
        rv = -1;
        goto main_end;
    }

    rv = read_neuron_config(args.conf_file);
    if (rv < 0) {
        log_error("failed to get neuron configuration.");
        rv = -1;
        goto main_end;
    }

    for (size_t i = 0; i < args.restart; ++i) {
        // flush log
        fflush(g_log_file);

        if ((pid = fork()) < 0) {
            log_error("cannot fork neuron daemon");
            goto main_end;
        } else if (pid == 0) { // child
            break;
        }

        // block waiting for child
        if (pid != waitpid(pid, &status, 0)) {
            log_error("cannot wait for neuron daemon");
            goto main_end;
        }

        if (WIFEXITED(status)) {
            status = WEXITSTATUS(status);
            log_info("detect neuron daemon exit with status:%d", status);
            if (NEU_RESTART_ONFAILURE == args.restart && 0 == status) {
                goto main_end;
            }
        } else if (WIFSIGNALED(status)) {
            signum = WTERMSIG(status);
            log_info("detect neuron daemon term with signal:%d", signum);
        }

        // sleep(1);
    }

    rv = neuron_run(&args);

main_end:
    uninit();
    neu_cli_args_fini(&args);
    return rv;
}
