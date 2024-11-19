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

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "core/manager.h"
#include "utils/log.h"
#include "utils/time.h"

#include "argparse.h"
#include "daemon.h"
#include "version.h"

#include "utils/cid.h"

static bool           exit_flag         = false;
static neu_manager_t *g_manager         = NULL;
zlog_category_t *     neuron            = NULL;
bool                  disable_jwt       = false;
bool                  sub_filter_err    = false;
int                   default_log_level = ZLOG_LEVEL_NOTICE;
char                  host_port[32]     = { 0 };
char                  g_status[32]      = { 0 };
static bool           sig_trigger       = false;

int64_t global_timestamp = 0;

struct {
    struct sockaddr_in addr;
    int                fd;
} g_remote_syslog_ctx;

static void sig_handler(int sig)
{
    nlog_warn("recv sig: %d", sig);

    if (!sig_trigger) {
        if (sig == SIGINT || sig == SIGTERM) {
            neu_manager_destroy(g_manager);
            neu_persister_destroy();
            zlog_fini();
        }
        sig_trigger = true;
    }
    exit_flag = true;
    exit(-1);
}

static inline char syslog_priority(const char *level)
{
    switch (level[0]) {
    case 'D': // DEBUG
        return '7';
    case 'I': // INFO
        return '6';
    case 'N': // NOTICE
        return '5';
    case 'W': // WARN
        return '4';
    case 'E': // ERROR
        return '3';
    case 'F': // FATAL
        return '2';
    default: // UNKNOWN
        return '1';
    }
}

static int remote_syslog(zlog_msg_t *msg)
{
    // fix priority
    msg->buf[1] = syslog_priority(msg->path);

    sendto(g_remote_syslog_ctx.fd, msg->buf, msg->len, 0,
           (const struct sockaddr *) &g_remote_syslog_ctx.addr,
           sizeof(g_remote_syslog_ctx.addr));
    return 0;
}

static int config_remote_syslog(const char *host, uint16_t port)
{
    g_remote_syslog_ctx.fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (g_remote_syslog_ctx.fd < 0) {
        return -1;
    }

    g_remote_syslog_ctx.addr.sin_family      = AF_INET;
    g_remote_syslog_ctx.addr.sin_port        = htons(port);
    g_remote_syslog_ctx.addr.sin_addr.s_addr = inet_addr(host);

    if (0 == inet_pton(AF_INET, host, &g_remote_syslog_ctx.addr.sin_addr)) {
        // not an valid ip address, try resolve as host name
        struct hostent *he = gethostbyname(host);
        if (NULL == he) {
            return -1;
        }

        memcpy(&g_remote_syslog_ctx.addr.sin_addr, he->h_addr_list[0],
               he->h_length);
    }

    zlog_set_record("remote_syslog", remote_syslog);
    return 0;
}

static int neuron_run(const neu_cli_args_t *args)
{
    struct rlimit rl = { 0 };
    int           rv = 0;

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGKILL, sig_handler);

    // try to enable core dump
    rl.rlim_cur = rl.rlim_max = RLIM_INFINITY;
    if (setrlimit(RLIMIT_CORE, &rl) < 0) {
        nlog_warn("neuron process failed enable core dump, ignore");
    }

    rv = neu_persister_create(args->config_dir);
    assert(rv == 0);

    zlog_notice(neuron, "neuron start, daemon: %d, version: %s (%s %s)",
                args->daemonized, NEURON_VERSION,
                NEURON_GIT_REV NEURON_GIT_DIFF, NEURON_BUILD_DATE);
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

    global_timestamp = neu_time_ms();
    neu_cli_args_init(&args, argc, argv);

    disable_jwt    = args.disable_auth;
    sub_filter_err = args.sub_filter_err;
    snprintf(host_port, sizeof(host_port), "http://%s:%d", args.ip, args.port);

    if (args.daemonized) {
        // become a daemon, this should be before calling `init`
        daemonize();
    }

    zlog_init(args.log_init_file);

    if (args.syslog_host && strlen(args.syslog_host) > 0 &&
        0 != config_remote_syslog(args.syslog_host, args.syslog_port)) {
        nlog_fatal("neuron setup remote syslog fail, exit.");
        goto main_end;
    }

    neuron = zlog_get_category("neuron");
    zlog_level_switch(neuron, default_log_level);

    cid_t cid   = { 0 };
    int   ret_c = neu_cid_parse("/tmp/UDT_531A_0011.ICD", &cid);
    // int ret_c = neu_cid_parse("/tmp/serverBMS_3.cid", &cid);
    printf("neu_cid_parse ret=%d\n", ret_c);
    for (int i = 0; i < cid.ied.n_access_points; i++) {
        printf("access_points[%d].name=%s\n", i, cid.ied.access_points[i].name);
        for (int j = 0; j < cid.ied.access_points[i].n_ldevices; j++) {
            printf("-- access_points[%d].ldevices[%d].inst=%s\n", i, j,
                   cid.ied.access_points[i].ldevices[j].inst);
            for (int k = 0; k < cid.ied.access_points[i].ldevices[j].n_lns;
                 k++) {
                printf(
                    "---- access_points[%d].ldevices[%d].lns[%d].lnprefix=%s\n",
                    i, j, k,
                    cid.ied.access_points[i].ldevices[j].lns[k].lnprefix);
                for (int l = 0;
                     l < cid.ied.access_points[i].ldevices[j].lns[k].n_dois;
                     l++) {
                    cid_doi_t *doi =
                        &cid.ied.access_points[i].ldevices[j].lns[k].dois[l];
                    if (doi->n_ctls > 0) {
                        for (int a = 0; a < doi->n_ctls; a++) {
                            printf("write ------ "
                                   "access_points[%d].ldevices[%d].lns[%d]."
                                   "dois[%d]"
                                   ".ctls[%d].da_name=%s, fc=%d, btype=%d\n",
                                   i, j, k, l, a, doi->ctls[a].da_name,
                                   doi->ctls[a].fc, doi->ctls[a].btype);
                        }
                    }
                }
                // for (int l = 0;
                // l < cid.ied.access_points[i].ldevices[j].lns[k].n_datasets;
                // l++) {
                // printf("------ "
                //"access_points[%d].ldevices[%d].lns[%d].datasets[%d]"
                //".name=%s\n",
                // i, j, k, l,
                // cid.ied.access_points[i]
                //.ldevices[j]
                //.lns[k]
                //.datasets[l]
                //.name);
                // for (int a = 0; a < cid.ied.access_points[i]
                //.ldevices[j]
                //.lns[k]
                //.datasets[l]
                //.n_fcda;
                // a++) {
                // cid_fcda_t *fcda = &cid.ied.access_points[i]
                //.ldevices[j]
                //.lns[k]
                //.datasets[l]
                //.fcdas[a];
                // if (fcda->n_btypes == 0) {
                // printf(
                //"nnn -------- "
                //"access_points[%d].ldevices[%d].lns[%d]"
                //".datasets[%d].fcdas[%d].name=%s, n type: %d\n",
                // i, j, k, l, a, fcda->do_name, fcda->n_btypes);
                //}
                //}
                //}
            }
        }
    }
    neu_cid_free(&cid);

    zlog_fini();
    return 0;

    if (neuron_already_running()) {
        if (args.stop) {
            rv = neuron_stop();
            nlog_notice("neuron stop ret=%d", rv);
            if (rv == 0) {
                printf("neuron stop successfully.\n");
            } else {
                printf("neuron stop failed.\n");
            }
        } else {
            nlog_fatal("neuron process already running, exit.");
            rv = -1;
        }
        goto main_end;
    } else {
        if (args.stop) {
            rv = 0;
            printf("neuron no running.\n");
            goto main_end;
        }
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
