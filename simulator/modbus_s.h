#ifndef SIMULATOR_MODBUS_S_H
#define SIMULATOR_MODBUS_S_H

#include <stdint.h>
#include <stdio.h>

void modbus_s_init();

ssize_t modbus_s_rtu_req(uint8_t *req, uint16_t req_len, uint8_t *res,
                         int res_mlen, int *res_len);
ssize_t modbus_s_tcp_req(uint8_t *req, uint16_t req_len, uint8_t *res,
                         int res_mlen, int *res_len);

#endif