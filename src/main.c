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
#include <nng/protocol/bus0/bus.h>
#include <nng/supplemental/util/platform.h>

#include "config.h"
#include "core/neu_manager.h"
#include "idhash.h"
#include "neu_log.h"
#include "neu_panic.h"
#include "restful/rest.h"

static neu_manager_t *manager;

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

static void init()
{
    nng_mtx_alloc(&log_mtx);
    log_set_lock(log_lock, log_mtx);
    log_set_level(LOG_DEBUG);
    g_logfile = fopen("neuron.log", "a");
    if (g_logfile == NULL) {
        fprintf(stderr,
                "Failed to open logfile when"
                "initialize neuron main process");
        abort();
    }
    // log_set_quiet(true);
    log_add_fp(g_logfile, LOG_DEBUG);
}

static void uninit()
{
    fclose(g_logfile);
    nng_mtx_free(log_mtx);
}

static void usage()
{
    log_info("neuron [--help] [--daemon]");
}

static int read_neuron_config()
{
    return neu_config_init("./neuron.yaml");
}

static void sig_handler(int sig)
{
    log_warn("recv sig: %d", sig);
    neu_manager_stop(manager);
}

int main(int argc, char *argv[])
{

    int  rv        = 0;
    bool is_daemon = false;

    init();

    char *        opts           = "h:d:";
    struct option long_options[] = {
        { "help", no_argument, NULL, 'h' },
        { "daemon", required_argument, NULL, 'd' },
    };
    char c;

    while ((c = getopt_long(argc, argv, opts, long_options, NULL)) != EOF) {
        switch (c) {
        case 'h':
            usage();
            exit(0);
        case 'd':
            is_daemon = true;
            break;
        default:
            log_warn("The arg %c is not supported!", c);
            break;
        }
    }

    if ((rv = read_neuron_config()) < 0) {
        log_error("Failed to get neuron configuration.");
        goto main_end;
    }

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGABRT, sig_handler);

    log_info("running neuron main process, daemon: %d", is_daemon);
    manager = neu_manager_create();
    if (manager == NULL) {
        log_error("Failed to create neuron manager, exit!");
        rv = -1;
        goto main_end;
    }

    neu_manager_destroy(manager);

main_end:
    uninit();
    return rv;
}
