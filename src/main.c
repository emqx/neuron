/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2022 EMQ Technologies Co., Ltd All rights reserved.
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

#include <signal.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#include "core/manager.h"
#include "utils/log.h"

#include "argparse.h"
#include "daemon.h"

static bool           exit_flag = false;
static neu_manager_t *g_manager = NULL;
zlog_category_t *     neuron    = NULL;

static void sig_handler(int sig)
{
    nlog_warn("recv sig: %d", sig);

    neu_manager_destroy(g_manager);
    zlog_fini();
    exit_flag = true;
}

static int neuron_run(const neu_cli_args_t *args)
{
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

    while (!exit_flag) {
        sleep(1);
    }

    return 0;
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
            nlog_notice("detect neuron daemon term with signal:%d", signum);
        }
    }

    rv = neuron_run(&args);

main_end:
    neu_cli_args_fini(&args);
    zlog_fini();
    return rv;
}
