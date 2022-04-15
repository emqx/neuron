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

#include <memory.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "log.h"

#include "modbus_point.h"

typedef struct modbus_point {
    modbus_area_e   area;
    uint8_t         device;
    uint16_t        addr;
    uint8_t         bit;
    modbus_endian_e endian;
    modbus_data_t   value;

    TAILQ_ENTRY(modbus_point) node;
} modbus_point_t;

typedef struct modbus_cmd {
    uint16_t          id;
    uint8_t           device;
    modbus_function_e function;
    uint16_t          start_addr;
    uint16_t          n_reg;

    uint16_t         n_point;
    modbus_point_t **points;
} modbus_cmd_t;

struct modbus_point_context {
    pthread_mutex_t mtx;
    TAILQ_HEAD(, modbus_point) points;

    uint16_t      cmd_id;
    uint16_t      n_cmd;
    modbus_cmd_t *cmds;

    void *arg;
};

static void insert_point(modbus_point_context_t *ctx, modbus_point_t *point);
static void insert_cmd(modbus_point_context_t *ctx, modbus_point_t *point);
static int  address_parse(char *addr, modbus_point_t *e);
static int  point_cmp(modbus_point_t *e1, modbus_point_t *e2);
static int  process_read_res(modbus_cmd_t *cmd, char *buf, ssize_t len);
static int  process_write_res(modbus_point_t *point, modbus_function_e function,
                              uint16_t n_reg, char *buf, ssize_t len);

modbus_point_context_t *modbus_point_init(void *arg)
{
    modbus_point_context_t *ctx =
        (modbus_point_context_t *) calloc(1, sizeof(modbus_point_context_t));

    pthread_mutex_init(&ctx->mtx, NULL);
    TAILQ_INIT(&ctx->points);

    ctx->cmd_id = 1;
    ctx->arg    = arg;

    return ctx;
}

int modbus_point_add(modbus_point_context_t *ctx, char *addr,
                     modbus_data_type_t type)
{
    modbus_point_t *point =
        (modbus_point_t *) calloc(1, sizeof(modbus_point_t));

    if (address_parse(addr, point) == 0) {
        switch (point->area) {
        case MODBUS_AREA_INPUT:
        case MODBUS_AREA_COIL:
            if (type != MODBUS_B8) {
                free(point);
                return -1;
            }
            break;
        case MODBUS_AREA_HOLD_REGISTER:
        case MODBUS_AREA_INPUT_REGISTER:
            if (type == MODBUS_B8) {
                free(point);
                return -1;
            }
            break;
        }
        point->value.type = type;
        insert_point(ctx, point);
    } else {
        log_error("modbus parse addr: %s error", addr);
        free(point);
        return -1;
    }

    log_info("add tag: %s success", addr);
    return 0;
}

void modbus_point_new_cmd(modbus_point_context_t *ctx)
{
    modbus_point_t *point = NULL;

    pthread_mutex_lock(&ctx->mtx);

    point = TAILQ_FIRST(&ctx->points);

    while (point != NULL) {
        insert_cmd(ctx, point);
        point = TAILQ_NEXT(point, node);
    }

    pthread_mutex_unlock(&ctx->mtx);
}

int modbus_point_get_cmd_size(modbus_point_context_t *ctx)
{
    int result = 0;

    pthread_mutex_lock(&ctx->mtx);
    result = ctx->n_cmd;
    pthread_mutex_unlock(&ctx->mtx);

    return result;
}

void modbus_point_clean(modbus_point_context_t *ctx)
{
    modbus_point_t *point = NULL;

    pthread_mutex_lock(&ctx->mtx);

    point = TAILQ_FIRST(&ctx->points);

    while (point != NULL) {
        TAILQ_REMOVE(&ctx->points, point, node);
        free(point);
        point = TAILQ_FIRST(&ctx->points);
    }

    for (int i = 0; i < ctx->n_cmd; i++) {
        free(ctx->cmds[i].points);
    }

    if (ctx->cmds != NULL) {
        free(ctx->cmds);
        ctx->cmds = NULL;
    }

    ctx->n_cmd = 0;

    pthread_mutex_unlock(&ctx->mtx);
}

void modbus_point_destory(modbus_point_context_t *ctx)
{
    modbus_point_clean(ctx);
    free(ctx);
}

int modbus_point_find(modbus_point_context_t *ctx, char *addr,
                      modbus_data_t *data)
{
    modbus_point_t  point;
    modbus_point_t *point_p = NULL;

    int ret = address_parse(addr, &point);
    if (ret != 0) {
        return -1;
    }

    point.value.type = data->type;

    pthread_mutex_lock(&ctx->mtx);

    ret = -1;
    TAILQ_FOREACH(point_p, &ctx->points, node)
    {
        if (point_cmp(point_p, &point) == 0) {
            switch (data->type) {
            case MODBUS_B8:
                data->val.val_u8 = point_p->value.val.val_u8;
                break;
            case MODBUS_B16:
                data->val.val_u16 = point_p->value.val.val_u16;
                break;
            case MODBUS_B32:
                data->val.val_u32 = point_p->value.val.val_u32;
                break;
            }
            ret = 0;
            break;
        }
    }

    pthread_mutex_unlock(&ctx->mtx);
    return ret;
}

int modbus_point_write(char *addr, modbus_data_t *data,
                       modbus_point_send_recv callback, void *arg)
{
    char              send_buf[64] = { 0 };
    char              recv_buf[64] = { 0 };
    ssize_t           send_len     = 0;
    ssize_t           recv_len     = 0;
    modbus_point_t    point        = { 0 };
    modbus_function_e function;
    int               ret = address_parse(addr, &point);

    if (ret != 0) {
        return -1;
    }

    switch (point.area) {
    case MODBUS_AREA_COIL:
        function = MODBUS_WRITE_M_COIL;
        break;
    case MODBUS_AREA_HOLD_REGISTER:
        function = MODBUS_WRITE_M_HOLD_REG;
        break;
    default:
        return -1;
    }

    send_len = modbus_m_write_req_with_head(
        send_buf, point.device, function, point.addr,
        data->type == MODBUS_B32 ? 2 : 1, data);

    recv_len = callback(arg, send_buf, send_len, recv_buf, sizeof(recv_buf));
    if (recv_len <= 0) {
        return -1;
    }

    return process_write_res(&point, function, data->type == MODBUS_B32 ? 2 : 1,
                             recv_buf, recv_len);
}

int modbus_point_all_read(modbus_point_context_t *ctx, bool with_head,
                          modbus_point_send_recv callback)
{
    char    send_buf[1500] = { 0 };
    char    recv_buf[1500] = { 0 };
    ssize_t send_len       = 0;
    ssize_t recv_len       = 0;
    int     result         = 0;
    int     ret;
    (void) with_head;

    pthread_mutex_lock(&ctx->mtx);
    for (int i = 0; i < ctx->n_cmd; i++) {
        send_len = modbus_read_req_with_head(
            send_buf, ctx->cmds[i].id, ctx->cmds[i].device,
            ctx->cmds[i].function, ctx->cmds[i].start_addr, ctx->cmds[i].n_reg);

        recv_len =
            callback(ctx->arg, send_buf, send_len, recv_buf, sizeof(recv_buf));
        if (recv_len <= 0) {
            log_error("cmd trans fail, id: %hd, function: %hd, addr: %hd, "
                      "n_reg: %hd",
                      ctx->cmds[i].id, ctx->cmds[i].function,
                      ctx->cmds[i].start_addr, ctx->cmds[i].n_reg);
            result = -1;
        } else {
            log_debug("cmd trans success, id: %hd, function: %hd, addr: %hd, "
                      "n_reg: %hd",
                      ctx->cmds[i].id, ctx->cmds[i].function,
                      ctx->cmds[i].start_addr, ctx->cmds[i].n_reg);
            ret = process_read_res(&ctx->cmds[i], recv_buf, recv_len);
            if (ret != 0) {
                result = -1;
                log_error("cmd parse error, id: %hd, function: %hd, addr: %hd, "
                          "n_reg: %hd",
                          ctx->cmds[i].id, ctx->cmds[i].function,
                          ctx->cmds[i].start_addr, ctx->cmds[i].n_reg);
            }
        }
    }

    pthread_mutex_unlock(&ctx->mtx);

    return result;
}

/*
**  modbus address format
**  1!400001
**  1!400001.1
**  1!400001#2
*/
static int address_parse(char *addr, modbus_point_t *e)
{
    int     n, n1, n2 = 0;
    char    ar;
    uint8_t en = -1;

    e->bit    = -1;
    e->endian = MODBUS_ENDIAN_UNDEFINE;

    n1 = sscanf(addr, "%hhd!%c%hd.%hhd", &e->device, &ar, &e->addr, &e->bit);
    n2 = sscanf(addr, "%hhd!%c%hd#%hhd", &e->device, &ar, &e->addr, &en);

    if (n2 > n1) {
        n = n2;
    } else {
        n = n1;
    }

    if (n != 3 && n != 4) {
        return -1;
    }

    e->endian = en;
    switch (ar) {
    case '0':
        e->area = MODBUS_AREA_COIL;
        break;
    case '1':
        e->area = MODBUS_AREA_INPUT;
        break;
    case '3':
        e->area = MODBUS_AREA_INPUT_REGISTER;
        break;
    case '4':
        e->area = MODBUS_AREA_HOLD_REGISTER;
        break;
    }

    e->addr -= 1;
    return 0;

    return 0;
}

static int point_cmp(modbus_point_t *e1, modbus_point_t *e2)
{
    if (e1->device > e2->device) {
        return 1;
    } else if (e1->device < e2->device) {
        return -1;
    }

    if (e1->area > e2->area) {
        return 1;
    } else if (e1->area < e2->area) {
        return -1;
    }

    if (e1->addr > e2->addr) {
        return 1;
    } else if (e1->addr < e2->addr) {
        return -1;
    }

    if (e1->value.type > e2->value.type) {
        return 1;
    } else if (e1->value.type < e2->value.type) {
        return -1;
    }

    return 0;
}

static void insert_point(modbus_point_context_t *ctx, modbus_point_t *point)
{
    modbus_point_t *p      = NULL;
    bool            insert = false;

    pthread_mutex_lock(&ctx->mtx);

    TAILQ_FOREACH(p, &ctx->points, node)
    {
        int ret = point_cmp(point, p);
        if (ret == 0) {
            TAILQ_INSERT_AFTER(&ctx->points, p, point, node);
            insert = true;
            break;
        } else if (ret == -1) {
            TAILQ_INSERT_BEFORE(p, point, node);
            insert = true;
            break;
        } else {
            continue;
        }
    }

    if (!insert) {
        TAILQ_INSERT_TAIL(&ctx->points, point, node);
    }

    pthread_mutex_unlock(&ctx->mtx);
}

static void insert_cmd(modbus_point_context_t *ctx, modbus_point_t *point)
{
    bool              exist    = false;
    modbus_function_e function = 0;

    switch (point->area) {
    case MODBUS_AREA_COIL:
        function = MODBUS_READ_COIL;
        break;
    case MODBUS_AREA_INPUT:
        function = MODBUS_READ_INPUT_CONTACT;
        break;
    case MODBUS_AREA_INPUT_REGISTER:
        function = MODBUS_READ_INPUT_REG;
        break;
    case MODBUS_AREA_HOLD_REGISTER:
        function = MODBUS_READ_HOLD_REG;
        break;
    }

    for (int i = 0; i < ctx->n_cmd; i++) {
        if (ctx->cmds[i].device == point->device &&
            ctx->cmds[i].function == function) {
            if (ctx->cmds[i].start_addr + ctx->cmds[i].n_reg == point->addr &&
                ctx->cmds[i].n_reg < 126) {
                exist = true;
                if (point->value.type == MODBUS_B32) {
                    ctx->cmds[i].n_reg += 2;
                } else {
                    ctx->cmds[i].n_reg += 1;
                }

                ctx->cmds[i].n_point += 1;
                ctx->cmds[i].points = (modbus_point_t **) realloc(
                    ctx->cmds[i].points,
                    ctx->cmds[i].n_point * sizeof(modbus_point_t *));

                ctx->cmds[i].points[ctx->cmds[i].n_point - 1] = point;
                return;
            }
            if (ctx->cmds[i].start_addr < point->addr &&
                ctx->cmds[i].start_addr + ctx->cmds[i].n_reg > point->addr) {
                ctx->cmds[i].n_point += 1;
                ctx->cmds[i].points = (modbus_point_t **) realloc(
                    ctx->cmds[i].points,
                    ctx->cmds[i].n_point * sizeof(modbus_point_t *));

                ctx->cmds[i].points[ctx->cmds[i].n_point - 1] = point;
                return;
            }
        }
    }

    if (!exist) {
        ctx->n_cmd += 1;
        ctx->cmds = (modbus_cmd_t *) realloc(ctx->cmds,
                                             ctx->n_cmd * sizeof(modbus_cmd_t));
        memset(&ctx->cmds[ctx->n_cmd - 1], 0, sizeof(modbus_cmd_t));

        ctx->cmds[ctx->n_cmd - 1].device     = point->device;
        ctx->cmds[ctx->n_cmd - 1].function   = function;
        ctx->cmds[ctx->n_cmd - 1].id         = ctx->cmd_id++;
        ctx->cmds[ctx->n_cmd - 1].start_addr = point->addr;
        ctx->cmds[ctx->n_cmd - 1].n_point    = 1;
        ctx->cmds[ctx->n_cmd - 1].points =
            (modbus_point_t **) calloc(1, sizeof(modbus_point_t *));
        ctx->cmds[ctx->n_cmd - 1].points[0] = point;
        if (point->value.type == MODBUS_B32) {
            ctx->cmds[ctx->n_cmd - 1].n_reg = 2;
        } else {
            ctx->cmds[ctx->n_cmd - 1].n_reg = 1;
        }
    }
}

static int process_read_res(modbus_cmd_t *cmd, char *buf, ssize_t len)
{
    struct modbus_header *           header = (struct modbus_header *) buf;
    struct modbus_code *             code   = (struct modbus_code *) &header[1];
    struct modbus_pdu_read_response *pdu =
        (struct modbus_pdu_read_response *) &code[1];
    uint8_t *data = (uint8_t *) &pdu[1];

    if (ntohs(header->process_no) != cmd->id) {
        return -1;
    }

    if (header->flag != 0x0000) {
        return -1;
    }

    if (ntohs(header->len) != len - sizeof(struct modbus_header)) {
        return -1;
    }

    if (code->device_address != cmd->device) {
        return -1;
    }

    if (code->function_code != cmd->function) {
        return -1;
    }

    if (cmd->function == MODBUS_READ_COIL ||
        cmd->function == MODBUS_READ_INPUT_CONTACT) {
        for (int i = 0; i < cmd->n_point; i++) {
            uint8_t *ptr =
                (uint8_t *) (data +
                             (cmd->points[i]->addr - cmd->start_addr) / 8);
            cmd->points[i]->value.val.val_u8 =
                (((*ptr) >> (cmd->points[i]->addr - cmd->start_addr) % 8) & 1) >
                0;
            log_info("get result bit.... %d, %d %d",
                     cmd->points[i]->value.val.val_u8, cmd->start_addr,
                     cmd->points[i]->addr);
        }
    }

    if (cmd->function == MODBUS_READ_HOLD_REG ||
        cmd->function == MODBUS_READ_INPUT_REG) {
        for (int i = 0; i < cmd->n_point; i++) {
            switch (cmd->points[i]->value.type) {
            case MODBUS_B16: {
                uint16_t *ptr =
                    (uint16_t *) ((uint16_t *) data + cmd->points[i]->addr -
                                  cmd->start_addr);
                cmd->points[i]->value.val.val_u16 = ntohs(*ptr);
                log_info("get result16.... %d, %d %d",
                         cmd->points[i]->value.val.val_u16, cmd->start_addr,
                         cmd->points[i]->addr);
                break;
            }
            case MODBUS_B32: {
                uint16_t *ptrl =
                    (uint16_t *) ((uint16_t *) data + cmd->points[i]->addr -
                                  cmd->start_addr);
                uint16_t *ptrh =
                    (uint16_t *) ((uint16_t *) data + cmd->points[i]->addr -
                                  cmd->start_addr + 1);
                cmd->points[i]->value.val.val_u32 =
                    ntohs(*ptrl) << 16 | ntohs(*ptrh);

                log_info("get result32.... %f, %d %d",
                         cmd->points[i]->value.val.val_f32, cmd->start_addr,
                         cmd->points[i]->addr);
                break;
            }
            default:
                break;
            }
        }
    }

    return 0;
}

static int process_write_res(modbus_point_t *point, modbus_function_e function,
                             uint16_t n_reg, char *buf, ssize_t len)
{
    struct modbus_header *            header = (struct modbus_header *) buf;
    struct modbus_code *              code = (struct modbus_code *) &header[1];
    struct modbus_pdu_write_response *pdu =
        (struct modbus_pdu_write_response *) &code[1];

    if (ntohs(header->process_no) != 0x0000) {
        return -1;
    }

    if (header->flag != 0x0000) {
        return -1;
    }

    if (ntohs(header->len) != len - sizeof(struct modbus_header)) {
        return -1;
    }

    if (code->device_address != point->device) {
        return -1;
    }

    if (code->function_code != function) {
        return -1;
    }

    if (ntohs(pdu->start_addr) != point->addr) {
        return -1;
    }

    if (ntohs(pdu->n_reg) != n_reg) {
        return -1;
    }

    log_info("write success, start addr: %d, n reg: %d", ntohs(pdu->start_addr),
             ntohs(pdu->n_reg));

    return 0;
}