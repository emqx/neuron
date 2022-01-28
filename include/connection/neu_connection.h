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

#ifndef NEURON_CONNECTION_H
#define NEURON_CONNECTION_H

#include <stdint.h>
#include <unistd.h>

typedef enum neu_conn_type {
    NEU_CONN_TCP_SERVER = 1,
    NEU_CONN_TCP_CLIENT,
    NEU_CONN_UDP,
    NEU_CONN_TTY_CLIENT,
} neu_conn_type_e;

typedef enum neu_conn_tty_baud {
    NEU_CONN_TTY_BAUD_115200,
    NEU_CONN_TTY_BAUD_57600,
    NEU_CONN_TTY_BAUD_38400,
    NEU_CONN_TTY_BAUD_19200,
    NEU_CONN_TTY_BAUD_9600,
} neu_conn_tty_baud_e;

typedef enum neu_conn_tty_parity {
    NEU_CONN_TTY_PARITY_NONE,
    NEU_CONN_TTY_PARITY_ODD,
    NEU_CONN_TTY_PARITY_EVEN,
    NEU_CONN_TTY_PARITY_MARK,
    NEU_CONN_TTY_PARITY_SPACE,
} neu_conn_tty_parity_e;

typedef enum neu_conn_tty_stop {
    NEU_CONN_TTY_STOP_1,
    NEU_CONN_TTY_STOP_2,
} neu_conn_tty_stop_e;

typedef enum neu_conn_tty_data {
    NEU_CONN_TTY_DATA_5,
    NEU_CONN_TTY_DATA_6,
    NEU_CONN_TTY_DATA_7,
    NEU_CONN_TTY_DATA_8,
} neu_conn_tty_data_e;

typedef void (*neu_conn_callback)(void *data, int fd);

typedef struct neu_conn_param {
    neu_conn_type_e type;

    union {
        struct {
            char *            ip;
            uint16_t          port;
            uint16_t          timeout; // millisecond
            int               max_link;
            neu_conn_callback start_listen;
            neu_conn_callback stop_listen;
        } tcp_server;

        struct {
            char *   ip;
            uint16_t port;
            uint16_t timeout; // millisecond
        } tcp_client;

        struct {
            char *   ip;
            uint16_t port;
        } udp;

        struct {
            char *                device;
            neu_conn_tty_data_e   data;
            neu_conn_tty_stop_e   stop;
            neu_conn_tty_baud_e   baud;
            neu_conn_tty_parity_e parity;
        } tty_client;
    } params;
} neu_conn_param_t;

typedef struct neu_conn neu_conn_t;

neu_conn_t *    neu_conn_new(neu_conn_param_t *param, void *data,
                             neu_conn_callback connected,
                             neu_conn_callback disconnected);
neu_conn_t *    neu_conn_reconfig(neu_conn_t *conn, neu_conn_param_t *param);
void            neu_conn_destory(neu_conn_t *conn);
neu_conn_type_e neu_conn_get_type(neu_conn_t *conn);
int             neu_conn_get_fd(neu_conn_t *conn);
int             neu_conn_tcp_accept(neu_conn_t *conn);
int             neu_conn_tcp_client_get_fd(neu_conn_t *conn);

void    neu_conn_tcp_flush(neu_conn_t *conn);
ssize_t neu_conn_send(neu_conn_t *conn, uint8_t *buf, ssize_t len);
ssize_t neu_conn_recv(neu_conn_t *conn, uint8_t *buf, ssize_t len);
#endif
