#ifndef _NEU_M_PLUGIN_MODBUS_STACK_H_
#define _NEU_M_PLUGIN_MODBUS_STACK_H_

#include <stdint.h>

#include <neuron.h>

#include "modbus.h"

typedef struct modbus_stack modbus_stack_t;

typedef int (*modbus_stack_send)(void *ctx, uint16_t n_byte, uint8_t *bytes);
typedef int (*modbus_stack_value)(void *ctx, uint8_t slave_id, uint16_t n_byte,
                                  uint8_t *bytes, int error);
typedef int (*modbus_stack_write_resp)(void *ctx, void *req, int error);

typedef enum modbus_protocol {
    MODBUS_PROTOCOL_TCP = 1,
    MODBUS_PROTOCOL_RTU = 2,
} modbus_protocol_e;

modbus_stack_t *modbus_stack_create(void *ctx, modbus_protocol_e protocol,
                                    modbus_stack_send       send_fn,
                                    modbus_stack_value      value_fn,
                                    modbus_stack_write_resp write_resp);
void            modbus_stack_destroy(modbus_stack_t *stack);

int modbus_stack_recv(modbus_stack_t *stack, neu_protocol_unpack_buf_t *buf);

int  modbus_stack_read(modbus_stack_t *stack, uint8_t slave_id,
                       enum modbus_area area, uint16_t start_address,
                       uint16_t n_reg, uint16_t *response_size);
int  modbus_stack_write(modbus_stack_t *stack, void *req, uint8_t slave_id,
                        enum modbus_area area, uint16_t start_address,
                        uint16_t n_reg, uint8_t *bytes, uint8_t n_byte,
                        uint16_t *response_size);
bool modbus_stack_is_rtu(modbus_stack_t *stack);

#endif