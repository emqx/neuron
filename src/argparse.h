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

#ifndef NEURON_ARGPARSE_H
#define NEURON_ARGPARSE_H

#define NEU_LOG_STDOUT_FNAME "/dev/stdout"

#define NEU_RESTART_NEVER 0
#define NEU_RESTART_ALWAYS ((size_t) -2)
#define NEU_RESTART_ONFAILURE ((size_t) -1)

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdlib.h>

/** Neuron command line arguments.
 */
typedef struct {
    bool   daemonized; // flag indicating whether to run as daemon process
    char * conf_file;  // configuration file path
    char * log_file;   // log file path
    size_t restart;    // restart policy
} neu_cli_args_t;

/** Parse command line arguments.
 */
void neu_cli_args_init(neu_cli_args_t *args, int argc, char *argv[]);

/** Clean up the command line arguments.
 */
void neu_cli_args_fini(neu_cli_args_t *args);

#ifdef __cplusplus
}
#endif

#endif
