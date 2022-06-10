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

#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zlog.h>

#include "argparse.h"
#include "neuron/version.h"

#define OPTIONAL_ARGUMENT_IS_PRESENT                             \
    ((optarg == NULL && optind < argc && argv[optind][0] != '-') \
         ? (bool) (optarg = argv[optind++])                      \
         : (optarg != NULL))

#define STRDUP(var, str)                                                       \
    do {                                                                       \
        (var) = strdup(str);                                                   \
        if (NULL == (var)) {                                                   \
            fprintf(                                                           \
                stderr,                                                        \
                "Neuron argument parser fail strdup string: %s, reason: %s\n", \
                (str), strerror(errno));                                       \
            goto quit;                                                         \
        }                                                                      \
    } while (0)

// clang-format off
const char *usage_text =
"USAGE:\n"
"    neuron [OPTIONS]\n\n"
"OPTIONS:\n"
"    -d, --daemon         run as daemon process\n"
"    -h, --help           show this help message\n"
"    --log                log to the stdout\n"
"    --restart <POLICY>   restart policy to apply when neuron daemon terminates,\n"
"                           - never,      never restart (default)\n"
"                           - always,     always restart\n"
"                           - on-failure, restart only if failure\n"
"                           - NUMBER,     restart max NUMBER of times\n"
"    --version            print version information\n"
"\n";
// clang-format on

static inline void usage()
{
    fprintf(stderr, "%s", usage_text);
}

static inline void version()
{
    printf("Neuron %s (%s %s)\n", NEURON_VERSION,
           NEURON_GIT_REV NEURON_GIT_DIFF, NEURON_BUILD_DATE);
}

static inline size_t parse_restart_policy(const char *s, size_t *out)
{
    if (0 == strcmp(s, "always")) {
        *out = NEU_RESTART_ALWAYS;
    } else if (0 == strcmp(s, "never")) {
        *out = NEU_RESTART_NEVER;
    } else if (0 == strcmp(s, "on-failure")) {
        *out = NEU_RESTART_ONFAILURE;
    } else {
        errno         = 0;
        char *    end = NULL;
        uintmax_t n   = strtoumax(s, &end, 0);
        // the entire string should be a number within range
        if (0 != errno || '\0' == *s || '\0' != *end ||
            n > NEU_RESTART_ALWAYS) {
            return -1;
        }
        *out = n;
    }

    return 0;
}

void neu_cli_args_init(neu_cli_args_t *args, int argc, char *argv[])
{
    char *        opts           = "c:dh";
    bool          dev_log        = false;
    struct option long_options[] = {
        { "help", no_argument, NULL, 1 },
        { "daemon", no_argument, NULL, 'd' },
        { "log", no_argument, NULL, 'l' },
        { "restart", required_argument, NULL, 'r' },
        { "version", no_argument, NULL, 'v' },
        { NULL, 0, NULL, 0 },
    };

    memset(args, 0, sizeof(*args));

    int c = 0;
    while ((c = getopt_long(argc, argv, opts, long_options, NULL)) != -1) {
        switch (c) {
        case 'h':
            usage();
            exit(0);
        case 'd':
            args->daemonized = true;
            break;
        case ':':
            fprintf(stderr, "%s: option '-%c' requires an argument\n", argv[0],
                    optopt);
            usage();
            goto quit;
        case 'l':
            zlog_init("./config/dev.conf");
            dev_log = true;
            break;
        case 'r':
            if (0 != parse_restart_policy(optarg, &args->restart)) {
                fprintf(stderr, "%s: option '--restart' invalid policy: `%s`\n",
                        argv[0], optarg);
                goto quit;
            }
            break;
        case 'v':
            version();
            goto quit;
            break;
        case '?':
        default:
            fprintf(stderr, "%s: option '-%c' is invalid: ignored\n", argv[0],
                    optopt);
            usage();
            goto quit;
        }
    }

    if (!dev_log) {
        zlog_init("./config/zlog.conf");
    }

    if (!args->daemonized && args->restart != NEU_RESTART_NEVER) {
        fprintf(stderr,
                "%s: option '--restart' has no effects without '--daemon'\n",
                argv[0]);
        args->restart = NEU_RESTART_NEVER;
    }

    return;

quit:
    neu_cli_args_fini(args);
    exit(1);
}

void neu_cli_args_fini(neu_cli_args_t *args)
{
    (void) args;
}
