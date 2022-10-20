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
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <zlog.h>

#include "argparse.h"
#include "persist/persist.h"
#include "version.h"

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

const char *g_config_dir = NULL;
const char *g_plugin_dir = NULL;

// clang-format off
const char *usage_text =
"USAGE:\n"
"    neuron [OPTIONS]\n\n"
"OPTIONS:\n"
"    -d, --daemon         run as daemon process\n"
"    -h, --help           show this help message\n"
"    --log                log to the stdout\n"
"    --reset-password     reset dashboard to use default password\n"
"    --restart <POLICY>   restart policy to apply when neuron daemon terminates,\n"
"                           - never,      never restart (default)\n"
"                           - always,     always restart\n"
"                           - on-failure, restart only if failure\n"
"                           - NUMBER,     restart max NUMBER of times\n"
"    --version            print version information\n"
"    --disable_auth       disable http api auth\n"
"    --config_dir <DIR>   directory from which neuron reads configuration\n"
"    --plugin_dir <DIR>   directory from which neuron loads plugin lib files\n"
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

static inline int reset_password()
{
    neu_persist_user_info_t info = {
        .name = "admin",
        .hash =
            "$5$PwFeXpBBIBZuZdZl$fP8fFPWCLoaWcnVXVSR.3Xi8TEqCvX92gjhowNNn6S4",
    };
    int rv = neu_persister_update_user(&info);
    return rv;
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

static inline bool file_exists(const char *const path)
{
    struct stat buf = { 0 };
    return -1 != stat(path, &buf);
}

void neu_cli_args_init(neu_cli_args_t *args, int argc, char *argv[])
{
    int           ret                 = 0;
    bool          reset_password_flag = false;
    char *        config_dir          = NULL;
    char *        plugin_dir          = NULL;
    char *        opts                = "dh";
    struct option long_options[]      = {
        { "help", no_argument, NULL, 'h' },
        { "daemon", no_argument, NULL, 'd' },
        { "log", no_argument, NULL, 'l' },
        { "reset-password", no_argument, NULL, 'r' },
        { "restart", required_argument, NULL, 'r' },
        { "version", no_argument, NULL, 'v' },
        { "disable_auth", no_argument, NULL, 'a' },
        { "config_dir", required_argument, NULL, 'c' },
        { "plugin_dir", required_argument, NULL, 'p' },
        { NULL, 0, NULL, 0 },
    };

    memset(args, 0, sizeof(*args));

    int c            = 0;
    int option_index = 0;
    while ((c = getopt_long(argc, argv, opts, long_options, &option_index)) !=
           -1) {
        switch (c) {
        case 'h':
            usage();
            goto quit;
        case 'd':
            args->daemonized = true;
            break;
        case ':':
            usage();
            goto quit;
        case 'l':
            args->dev_log = true;
            break;
        case 'r':
            if (0 ==
                strcmp("reset-password", long_options[option_index].name)) {
                reset_password_flag = true;
            } else if (0 != parse_restart_policy(optarg, &args->restart)) {
                fprintf(stderr, "%s: option '--restart' invalid policy: `%s`\n",
                        argv[0], optarg);
                ret = 1;
                goto quit;
            }
            break;
        case 'v':
            version();
            goto quit;
        case 'a':
            args->disable_auth = true;
            break;
        case 'c':
            config_dir = strdup(optarg);
            break;
        case 'p':
            plugin_dir = strdup(optarg);
            break;
        case '?':
        default:
            usage();
            ret = 1;
            goto quit;
        }
    }

    if (!args->daemonized && args->restart != NEU_RESTART_NEVER) {
        fprintf(stderr,
                "%s: option '--restart' has no effects without '--daemon'\n",
                argv[0]);
        args->restart = NEU_RESTART_NEVER;
    }

    args->config_dir = config_dir ? config_dir : strdup("./config");
    if (!file_exists(args->config_dir)) {
        fprintf(stderr, "configuration directory `%s` not exists\n",
                args->config_dir);
        ret = 1;
        goto quit;
    }

    args->plugin_dir = plugin_dir ? plugin_dir : strdup("./plugins");
    if (!file_exists(args->plugin_dir)) {
        fprintf(stderr, "plugin directory `%s` not exists\n", args->plugin_dir);
        ret = 1;
        goto quit;
    }

    const char *zlog_conf = args->dev_log ? "dev.conf" : "zlog.conf";
    int         n = 1 + snprintf(NULL, 0, "%s/%s", args->config_dir, zlog_conf);
    char *      log_init_file = malloc(n);
    snprintf(log_init_file, n, "%s/%s", args->config_dir, zlog_conf);
    args->log_init_file = log_init_file;
    if (!file_exists(args->log_init_file)) {
        fprintf(stderr, "log init file `%s` not exists\n", args->log_init_file);
        ret = 1;
        goto quit;
    }

    // passing information by global variable is not a good style
    g_config_dir = args->config_dir;
    g_plugin_dir = args->plugin_dir;

    if (reset_password_flag) {
        ret = reset_password();
        goto quit;
    }

    return;

quit:
    neu_cli_args_fini(args);
    exit(ret);
}

void neu_cli_args_fini(neu_cli_args_t *args)
{
    if (args) {
        free(args->log_init_file);
        free(args->config_dir);
        free(args->plugin_dir);
    }
}
