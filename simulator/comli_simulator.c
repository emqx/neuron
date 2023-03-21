#include <memory.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <neuron.h>

#include "plugins/comli/comli.h"

#define UNUSED(X) ((void) (X))

zlog_category_t *neuron           = NULL;
neu_events_t *   events           = NULL;
neu_event_io_t * event            = NULL;
neu_conn_t *     conn             = NULL;
int64_t          global_timestamp = 0;
uint8_t          identity         = 0;

static void connected(void *data, int fd);
static void disconnected(void *data, int fd);

uint8_t  IOBITS[2048]    = { 0 };
uint16_t REGISTERS[3072] = { 0 };

uint8_t calcbcc(uint8_t *data, uint32_t len)
{
    uint8_t bcc = 0;
    for (uint32_t i = 0; i < len; i++) {
        bcc ^= data[i];
    }

    return bcc;
}

void mirror(uint8_t *ch)
{

    uint8_t t = 0x00;
    for (uint32_t i = 0; i < 8; i++) {
        t <<= 1;
        t |= *ch & 0x01;
        *ch >>= 1;
    }
    *ch = t;
}

void hex_str_to_binary_u8(char *in, uint8_t *out)
{
    char buf[3] = { 0 };
    memcpy(buf, in, 2);
    sscanf(buf, "%2hhX", out);
}

void hex_str_to_binary_u16(char *in, uint16_t *out)
{
    char buf[5] = { 0 };
    memcpy(buf, in, 4);
    sscanf(buf, "%4hX", out);
}

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

    UNUSED(fd);

    if (type != NEU_EVENT_IO_READ) {
        nlog_warn("read close!");
        return -1;
    }
    neu_conn_t *tconn      = (neu_conn_t *) (usr_data);
    uint8_t     buffer[77] = { 0 };

    int sb_fb_size =
        sizeof(struct comli_start_block) + sizeof(struct comli_function_block);

    int ret = neu_conn_recv(tconn, buffer, sb_fb_size);

    if (ret < sb_fb_size) {
        nlog_warn("recv fail!");
        return -1;
    }

    struct comli_start_block *   sb = (struct comli_start_block *) buffer;
    struct comli_function_block *fb = (struct comli_function_block *) &sb[1];

    if (sb->stx != COMLI_STX) {
        uint8_t maxbuff[1024] = { 0 };
        neu_conn_recv(tconn, maxbuff, 1024);
        return -1;
    }

    uint8_t tidentity = 0;
    hex_str_to_binary_u8((char *) &sb->identity, &tidentity);
    if (identity != tidentity) {
        return -1;
    }

    if ((sb->stamp != '1') && (sb->stamp != '2') && (sb->stamp != '0')) {
        return -1;
    }

    uint16_t quantity = 0;
    hex_str_to_binary_u16((char *) &fb->quantity, &quantity);

    uint8_t respbuf[77] = { 0 };

    uint16_t address = 0;
    hex_str_to_binary_u16((char *) &fb->address, &address);

    if (fb->message_type == COMLI_REQ_SEVERAL_IOBITS_OR_REGISTERS) {

        ret = neu_conn_recv(tconn, buffer + sb_fb_size, 2);

        if (ret < 2) {
            nlog_warn("recv fail!");
            return -1;
        }

        struct comli_end_block *eb = (struct comli_end_block *) &fb[1];

        if (eb->etx != COMLI_ETX) {
            return -1;
        }

        uint8_t bcc = calcbcc(buffer, 12);

        if (bcc != eb->bcc) {
            return -1;
        }

        struct comli_start_block *resp_sb =
            (struct comli_start_block *) respbuf;
        struct comli_function_block *resp_fb =
            (struct comli_function_block *) &resp_sb[1];

        resp_sb->stx      = COMLI_STX;
        resp_sb->identity = COMLI_MASTER_ID;
        resp_sb->stamp    = sb->stamp;

        resp_fb->message_type = COMLI_TRANSFER_IOBITS_OR_REGISTERS;

        resp_fb->address  = fb->address;
        resp_fb->quantity = fb->quantity;

        if (address < 0x4000) {
            memcpy(resp_fb->data, IOBITS + address, quantity);
        } else {
            address = (address - 0x4000) / 16;
            memcpy(resp_fb->data, REGISTERS + address, quantity);
        }

        struct comli_end_block *resp_eb =
            (struct comli_end_block *) (resp_fb->data + quantity);

        for (uint32_t i = 0; i < quantity; i++) {
            mirror(&resp_fb->data[i]);
        }

        resp_eb->etx = COMLI_ETX;

        uint8_t resp_bcc = calcbcc(respbuf, 12 + quantity);

        resp_eb->bcc = resp_bcc;

        ret = neu_conn_send(tconn, respbuf, 13 + quantity);

        if (ret != 13 + quantity) {
            printf("send fail!");
        }
    } else if (fb->message_type == COMLI_TRANSFER_IOBITS_OR_REGISTERS) {

        ret = neu_conn_recv(tconn, buffer + sb_fb_size, quantity + 2);

        if (ret < quantity + 2) {
            nlog_warn("recv fail!");
            return -1;
        }

        struct comli_end_block *eb =
            (struct comli_end_block *) (fb->data + quantity);

        if (eb->etx != COMLI_ETX) {
            return -1;
        }

        uint8_t bcc = calcbcc(buffer, 12 + quantity);

        if (bcc != eb->bcc) {
            return -1;
        }

        struct comli_start_block *resp_sb =
            (struct comli_start_block *) respbuf;

        resp_sb->stx      = COMLI_STX;
        resp_sb->identity = COMLI_MASTER_ID;
        resp_sb->stamp    = sb->stamp;

        for (uint32_t i = 0; i < quantity; i++) {
            mirror(&fb->data[i]);
        }

        if (address < 0x4000) {
            memcpy(IOBITS + address, fb->data, quantity);
        } else {
            address = (address - 0x4000) / 16;
            memcpy(REGISTERS + address, fb->data, quantity);
        }

        respbuf[4] = COMLI_ACK;
        respbuf[5] = 0x06;
        respbuf[6] = COMLI_ETX;

        uint8_t resp_bcc = calcbcc(respbuf, 7);

        respbuf[7] = resp_bcc;

        ret = neu_conn_send(tconn, respbuf, 8);

        if (ret != 8) {
            printf("send fail!");
        }
    } else if (fb->message_type == COMLI_TRANSFER_INDIVIDUAL_IOBIT) {
        ret = neu_conn_recv(tconn, buffer + sb_fb_size, quantity + 2);

        if (ret < quantity + 2) {
            nlog_warn("recv fail!");
            return -1;
        }

        struct comli_end_block *eb =
            (struct comli_end_block *) (fb->data + quantity);

        if (eb->etx != COMLI_ETX) {
            return -1;
        }

        uint8_t bcc = calcbcc(buffer, 12 + quantity);

        if (bcc != eb->bcc) {
            return -1;
        }

        struct comli_start_block *resp_sb =
            (struct comli_start_block *) respbuf;

        resp_sb->stx      = COMLI_STX;
        resp_sb->identity = COMLI_MASTER_ID;
        resp_sb->stamp    = sb->stamp;

        for (uint32_t i = 0; i < quantity; i++) {
            mirror(&fb->data[i]);
        }

        if (address < 0x4000) {
            uint8_t base_address = address / 8;
            uint8_t bit_address  = address % 8;
            if (fb->data[0] == 0x30) {
                IOBITS[base_address] =
                    IOBITS[base_address] & (~(0x01 << bit_address));
            } else {
                IOBITS[base_address] =
                    IOBITS[base_address] | (0x01 << bit_address);
            }
        }

        respbuf[4] = COMLI_ACK;
        respbuf[5] = 0x06;
        respbuf[6] = COMLI_ETX;

        uint8_t resp_bcc = calcbcc(respbuf, 7);

        respbuf[7] = resp_bcc;

        ret = neu_conn_send(tconn, respbuf, 8);

        if (ret != 8) {
            printf("send fail!");
        }
    }
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        printf("./comli_simulator device identity\n");
        return -1;
    }

    char *device = argv[1];
    identity     = atoi(argv[2]);
    if (identity > 248) {
        printf("identity <= 247\n");
        return -1;
    }

    zlog_init("./config/dev.conf");
    neuron                 = zlog_get_category("neuron");
    neu_conn_param_t param = {
        .log                       = neuron,
        .type                      = NEU_CONN_TTY_CLIENT,
        .params.tty_client.device  = device,
        .params.tty_client.baud    = NEU_CONN_TTY_BAUD_9600,
        .params.tty_client.data    = NEU_CONN_TTY_DATA_8,
        .params.tty_client.parity  = NEU_CONN_TTY_PARITY_NONE,
        .params.tty_client.stop    = NEU_CONN_TTY_STOP_1,
        .params.tty_client.timeout = 3000,
    };

    events = neu_event_new();
    conn   = neu_conn_new(&param, NULL, connected, disconnected);

    neu_conn_start(conn);

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
    (void) fd;
    return;
}

static void disconnected(void *data, int fd)
{
    (void) data;
    (void) fd;

    return;
}
