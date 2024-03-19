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
#include <getopt.h>
#include <memory.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <neuron.h>

#include "modbus_s.h"

#define UNUSED(X) ((void) (X))
#define BUFFER_SIZE (4096)

zlog_category_t *neuron = NULL;
neu_events_t *   events = NULL;
neu_event_io_t * event  = NULL;
neu_conn_t *     conn   = NULL;

struct cycle_buf {
    int     len;
    uint8_t buf[BUFFER_SIZE];
};

static struct cycle_buf recv_buf, res_buf;

static void connected(void *data, int fd);
static void disconnected(void *data, int fd);

static void sig_handler(int sig)
{
    (void) sig;

    nlog_warn("recv sig: %d\n", sig);
    neu_conn_destory(conn);
    neu_event_close(events);
    exit(0);
}

static int slave_io_cb(enum neu_event_io_type type, int fd, void *usr_data)
{
    UNUSED(usr_data);
    if (type != NEU_EVENT_IO_READ) {
        nlog_warn("read close!");
        return -1;
    }

    int ret = read(fd, recv_buf.buf + recv_buf.len, BUFFER_SIZE - recv_buf.len);

    if (ret <= 0) {
        nlog_warn("recv failed!");
        return -1;
    }

    recv_buf.len += ret;

    ret = modbus_s_rtu_req(recv_buf.buf, recv_buf.len, res_buf.buf, BUFFER_SIZE,
                           &res_buf.len);

    if (ret == -2) {
        printf("recv modbus adrress for retry_test, continue wait msg\n");
        return 0;
    }

    neu_msleep(2);

    ret = write(fd, res_buf.buf, res_buf.len);
    if (ret <= 0) {
        nlog_warn("send failed!");
        return -1;
    }

    recv_buf.len = 0;

    return 0;
}

static inline void usage(const char *prog)
{
    // clang-format off
    const char *text =
    "USAGE:\n"
    "    %s [OPTIONS]\n\n"
    "OPTIONS:\n"
    "    -l, --link           TTY device\n"
    "    -b, --baud           TTY baud rate (default 9600)\n"
    "    -p, --parity         TTY parity (default none)\n"
    "    -s, --stop           TTY stop bits (default 0)\n"
    "    -d, --data           TTY data bits (default 8)\n"
    "    -f, --flow           Enable TTYP flow\n"
    "    -e, --simulate_error Simulate error\n"
    "    -h, --help           show this help message\n"
    "\n";
    // clang-format on

    fprintf(stderr, text, prog);
}

int main(int argc, char *argv[])
{
    char *                device = NULL;
    neu_conn_tty_baud_e   baud   = NEU_CONN_TTY_BAUD_9600;
    neu_conn_tty_parity_e parity = NEU_CONN_TTY_PARITY_NONE;
    neu_conn_tty_stop_e   stop   = NEU_CONN_TTY_STOP_1;
    neu_conn_tty_data_e   data   = NEU_CONN_TTY_DATA_8;
    neu_conn_tty_flow_e   flow   = NEU_CONN_TTYP_FLOW_DISABLE;
    bool                  sim    = false;

    struct option long_options[] = {
        { "help", no_argument, NULL, 'h' },
        { "link", required_argument, NULL, 'l' },
        { "baud", required_argument, NULL, 'b' },
        { "parity", required_argument, NULL, 'p' },
        { "stop", required_argument, NULL, 's' },
        { "data", required_argument, NULL, 'd' },
        { "flow", no_argument, NULL, 'f' },
        { "simulate_error", no_argument, NULL, 'e' },
        { NULL, 0, NULL, 0 },
    };

    int c            = 0;
    int option_index = 0;

    while ((c = getopt_long(argc, argv, "hl:b:p:s:d:fe", long_options,
                            &option_index)) != -1) {
        switch (c) {
        case 'h':
            usage(argv[0]);
            return 0;
        case 'l':
            device = optarg;
            break;
        case 'b': {
            int t_baud = atoi(optarg);
            switch (t_baud) {
            case 2400:
                baud = NEU_CONN_TTY_BAUD_2400;
                break;
            case 4800:
                baud = NEU_CONN_TTY_BAUD_4800;
                break;
            case 9600:
                baud = NEU_CONN_TTY_BAUD_9600;
                break;
            case 19200:
                baud = NEU_CONN_TTY_BAUD_19200;
                break;
            case 38400:
                baud = NEU_CONN_TTY_BAUD_38400;
                break;
            case 57600:
                baud = NEU_CONN_TTY_BAUD_57600;
                break;
            case 115200:
                baud = NEU_CONN_TTY_BAUD_115200;
                break;
            default:
                printf("baud no match!input:2400 4800 9600 19200 38400 57600 "
                       "115200\n");
                return -1;
            }
            break;
        }
        case 'p': {
            if (strcmp(optarg, "none") == 0) {
                parity = NEU_CONN_TTY_PARITY_NONE;
            } else if (strcmp(optarg, "odd") == 0) {
                parity = NEU_CONN_TTY_PARITY_ODD;
            } else if (strcmp(optarg, "even") == 0) {
                parity = NEU_CONN_TTY_PARITY_EVEN;
            } else if (strcmp(optarg, "mark") == 0) {
                parity = NEU_CONN_TTY_PARITY_MARK;
            } else if (strcmp(optarg, "space") == 0) {
                parity = NEU_CONN_TTY_PARITY_SPACE;
            } else {
                printf("parity no match!input:none odd even mark space\n");
                return -1;
            }
            break;
        }
        case 's': {
            int t_stop = atoi(optarg);
            switch (t_stop) {
            case 1:
                stop = NEU_CONN_TTY_STOP_1;
                break;
            case 2:
                stop = NEU_CONN_TTY_STOP_2;
                break;
            default:
                printf("stop no match!input:1 2\n");
                return -1;
            }
            break;
        }
        case 'd': {
            int t_data = atoi(optarg);
            switch (t_data) {
            case 5:
                data = NEU_CONN_TTY_DATA_5;
                break;
            case 6:
                data = NEU_CONN_TTY_DATA_6;
                break;
            case 7:
                data = NEU_CONN_TTY_DATA_7;
                break;
            case 8:
                data = NEU_CONN_TTY_DATA_8;
                break;
            default:
                printf("data no match!input:5 6 7 8");
                return -1;
            }
            break;
        }
        case 'f': {
            flow = NEU_CONN_TTYP_FLOW_ENABLE;
            break;
        }
        case 'e': {
            sim = true;
            break;
        }
        case '?':
        default:
            usage(argv[0]);
            return -1;
        }
    }

    zlog_init("./config/dev.conf");
    neuron = zlog_get_category("neuron");
    modbus_s_init();
    modbus_s_simulate_error(sim);
    neu_conn_param_t param = {
        .log                       = neuron,
        .type                      = NEU_CONN_TTY_CLIENT,
        .params.tty_client.device  = device,
        .params.tty_client.baud    = baud,
        .params.tty_client.data    = data,
        .params.tty_client.parity  = parity,
        .params.tty_client.stop    = stop,
        .params.tty_client.flow    = flow,
        .params.tty_client.timeout = 3000,
    };

    events = neu_event_new();
    conn   = neu_conn_new(&param, NULL, connected, disconnected);

    neu_conn_start(conn);

    neu_conn_connect(conn);

    int fd = neu_conn_fd(conn);

    if (fd <= 0) {
        return -1;
    }

    neu_event_io_param_t ioparam = { 0 };
    ioparam.fd                   = fd;
    ioparam.cb                   = slave_io_cb;
    ioparam.usr_data             = conn;

    event = neu_event_add_io(events, ioparam);

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGABRT, sig_handler);

    while (1) {
        sleep(10);
    }

    return 0;
}

static void connected(void *data, int fd)
{
    (void) data;
    nlog_debug("connected %d", fd);
    return;
}

static void disconnected(void *data, int fd)
{
    (void) data;
    nlog_debug("disconnected %d", fd);
    return;
}