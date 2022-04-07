#include <arpa/inet.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "connection/neu_tcp.h"
#include "log.h"
#include "plugins/modbus/modbus.h"

static bool run = true;

static void sig_handle(int sig)
{
    log_warn("recv sig: %d", sig);
    run = false;
}

static uint8_t  coil[65535]      = { 0 };
static uint8_t  input[65535]     = { 0 };
static uint16_t input_reg[65535] = { 0 };
static uint16_t hold_reg[65535]  = { 0 };

void input_init()
{
    for (uint32_t i = 0; i < 65535; i++) {
        if (i % 2 == 0) {
            input[i] = 0;
        } else {
            input[i] = 1;
        }
    }

    for (uint32_t i = 0; i < 65535; i++) {
        input_reg[i] = (uint16_t) i;
    }
}

int main(int argc, char *argv[])
{
    uint16_t port = 60502;

    if (argc == 2) {
        int t_port = atoi(argv[1]);
        if (t_port > 0) {
            port = t_port;
        }
    }

    neu_tcp_server_t *server     = neu_tcp_server_create("127.0.0.1", port);
    bool              new_client = true;
    int               ret        = 0;

    if (server == NULL) {
        return -1;
    }

    input_init();

    signal(SIGINT, sig_handle);
    signal(SIGTERM, sig_handle);
    signal(SIGABRT, sig_handle);

    while (run) {
        char     buf[64]        = { 0 };
        char     body_buf[1024] = { 0 };
        char     response[4096] = { 0 };
        ssize_t  res_len        = 0;
        ssize_t  len            = 0;
        uint8_t *w_u8           = NULL;

        struct modbus_header *          header = (struct modbus_header *) buf;
        struct modbus_code *            code  = (struct modbus_code *) body_buf;
        struct modbus_pdu_read_request *r_req = NULL;
        struct modbus_pdu_m_write_request *w_req = NULL;

        struct modbus_header *res_header = (struct modbus_header *) response;
        struct modbus_code *  res_code = (struct modbus_code *) &res_header[1];
        struct modbus_pdu_read_response * r_res = NULL;
        struct modbus_pdu_write_response *w_res = NULL;

        res_len = sizeof(struct modbus_header) + sizeof(struct modbus_code);

        if (new_client) {
            ret = neu_tcp_server_wait_client(server);
            if (ret != 0) {
                sleep(3);
                continue;
            } else {
                new_client = false;
            }
        }

        len = neu_tcp_server_recv(server, (char *) header,
                                  sizeof(struct modbus_header),
                                  sizeof(struct modbus_header));
        if (len <= 0) {
            log_warn("recv header error, %d", len);
            new_client = true;
            sleep(1);
            continue;
        }

        if (ntohs(header->len) <= 2) {
            log_warn("head len error, %d", ntohs(header->len));
            new_client = true;
            sleep(1);
            continue;
        }

        len = neu_tcp_server_recv(server, body_buf, sizeof(body_buf),
                                  ntohs(header->len));
        if (len <= 0) {
            log_warn("recv code error, %d", len);
            new_client = true;
            sleep(1);
            continue;
        }

        switch (code->function_code) {
        case MODBUS_READ_COIL:
        case MODBUS_READ_INPUT_CONTACT:
        case MODBUS_READ_HOLD_REG:
        case MODBUS_READ_INPUT_REG:
            r_req = (struct modbus_pdu_read_request *) &code[1];
            r_res = (struct modbus_pdu_read_response *) &res_code[1];
            res_len += sizeof(struct modbus_pdu_read_response);
            break;
        case MODBUS_WRITE_S_COIL:
        case MODBUS_WRITE_S_HOLD_REG:
        case MODBUS_WRITE_M_HOLD_REG:
        case MODBUS_WRITE_M_COIL:
            w_req = (struct modbus_pdu_m_write_request *) &code[1];
            w_res = (struct modbus_pdu_write_response *) &res_code[1];
            w_u8  = (uint8_t *) &w_req[1];
            res_len += sizeof(struct modbus_pdu_write_response);
            break;
        }

        switch (code->function_code) {
        case MODBUS_READ_COIL:
        case MODBUS_READ_INPUT_CONTACT: {
            r_res->n_byte = ntohs(r_req->n_reg) / 8;
            if (ntohs(r_req->n_reg) % 8 != 0) {
                r_res->n_byte += 1;
            }

            uint8_t *byte = (uint8_t *) &r_res[1];

            for (int i = 0; i < ntohs(r_req->n_reg); i++) {
                if (code->function_code == MODBUS_READ_COIL) {
                    byte[i / 8] = byte[i / 8] |
                        coil[ntohs(r_req->start_addr) + i] << (i % 8);
                    log_info("read coil, addr: %d, value: %d, index: %d",
                             ntohs(r_req->start_addr) + i, byte[i / 8], i / 8);
                } else {
                    byte[i / 8] = byte[i / 8] |
                        input[ntohs(r_req->start_addr) + i] << (i % 8);
                    log_info("read, addr: %d, i: %d, value: %d, n_reg: %d, "
                             "byte: %d, "
                             "dvluae: %d",
                             ntohs(r_req->start_addr) + i, i,
                             input[ntohs(r_req->start_addr) + i],
                             ntohs(r_req->n_reg), byte[i / 8],
                             input[ntohs(r_req->start_addr) + i] << (i % 8));
                }
            }

            res_len += r_res->n_byte;

            break;
        }
        case MODBUS_READ_HOLD_REG:
        case MODBUS_READ_INPUT_REG:
            r_res->n_byte = ntohs(r_req->n_reg) * 2;

            uint16_t *byte = (uint16_t *) &r_res[1];

            for (int i = 0; i < ntohs(r_req->n_reg); i++) {
                if (code->function_code == MODBUS_READ_HOLD_REG) {
                    byte[i] = htons(hold_reg[ntohs(r_req->start_addr) + i]);
                } else {
                    byte[i] = htons(input_reg[ntohs(r_req->start_addr) + i]);
                }
                // log_info("read addr: %d, value: %d, i:%d",
                // ntohs(r_req->start_addr) + i, byte[i], i);
            }

            res_len += r_res->n_byte;
            //            log_info("read resp: n_byte: %d,len :%d",
            //            r_res->n_byte, res_len);

            break;
        case MODBUS_WRITE_S_COIL:
        case MODBUS_WRITE_S_HOLD_REG:
            break;
        case MODBUS_WRITE_M_HOLD_REG:
            w_res->start_addr = w_req->start_addr;
            w_res->n_reg      = w_req->n_reg;

            for (int i = 0; i < w_req->n_byte; i++) {
                hold_reg[ntohs(w_req->start_addr) + i / 2] = 0;
            }

            for (int i = 0; i < w_req->n_byte; i++) {
                if (i % 2 == 0) {
                    hold_reg[ntohs(w_req->start_addr) + i / 2] =
                        hold_reg[ntohs(w_req->start_addr) + i / 2] |
                        (w_u8[i] << 8);
                } else {
                    hold_reg[ntohs(w_req->start_addr) + i / 2] =
                        hold_reg[ntohs(w_req->start_addr) + i / 2] | w_u8[i];
                }
                log_info("write coil addr: %d, value: %X, reg: %d",
                         ntohs(w_req->start_addr) + i / 2, w_u8[i],
                         hold_reg[ntohs(w_req->start_addr) + i / 2]);
            }

            res_len += sizeof(struct modbus_pdu_write_response);
            break;
        case MODBUS_WRITE_M_COIL: {
            w_res->start_addr = w_req->start_addr;
            w_res->n_reg      = w_req->n_reg;

            for (int i = 0; i < ntohs(w_req->n_reg); i++) {
                uint8_t x = (w_u8[ntohs(w_req->n_reg) / 8] >> (i % 8)) & 0x1;

                if (x > 0) {
                    coil[ntohs(w_req->start_addr) + i] = 1;
                } else {
                    coil[ntohs(w_req->start_addr) + i] = 0;
                }
                log_info("write coil addr: %d, value: %d",
                         ntohs(w_req->start_addr) + i, x > 0 ? 1 : 0);
            }

            res_len += sizeof(struct modbus_pdu_write_response);
            break;
        }
        }

        res_header->process_no = header->process_no;
        res_header->flag       = header->flag;
        res_header->len        = htons(res_len - sizeof(struct modbus_header));
        res_code->device_address = code->device_address;
        res_code->function_code  = code->function_code;

        len = neu_tcp_server_send(server, response, res_len);
        if (len <= 0) {
            log_warn("send response %d", res_len);
            new_client = true;
            sleep(1);
            continue;
        }
    }

    neu_tcp_server_close(server);

    return 0;
}
