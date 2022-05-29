#ifndef _NEU_M_PLUGIN_MODBUS_REQ_H_
#define _NEU_M_PLUGIN_MODBUS_REQ_H_

#include <neuron.h>

#include "modbus_stack.h"

struct neu_plugin {
    neu_plugin_common_t common;

    neu_conn_t *    conn;
    modbus_stack_t *stack;

    void *   plugin_group_data;
    uint16_t cmd_idx;

    neu_event_io_t *tcp_server_io;
    int             client_fd;
    neu_events_t *  events;
};

void modbus_conn_connected(void *data, int fd);
void modbus_conn_disconnected(void *data, int fd);

int modbus_group_timer(neu_plugin_t *plugin, neu_plugin_group_t *group,
                       uint16_t max_byte);
int modbus_send_msg(void *ctx, uint16_t n_byte, uint8_t *bytes);
int modbus_value_handle(void *ctx, uint8_t slave_id, uint16_t n_byte,
                        uint8_t *bytes);
int modbus_write(neu_plugin_t *plugin, void *req, neu_datatag_t *tag,
                 neu_value_u value);
int modbus_write_resp(void *ctx, void *req, int error);

#endif
