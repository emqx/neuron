#ifndef _NEU_PLUGIN_MODBUS_POINT_H_
#define _NEU_PLUGIN_MODBUS_POINT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include <neuron.h>

#include "modbus.h"

typedef struct modbus_point {
    uint8_t       slave_id;
    modbus_area_e area;
    uint16_t      start_address;
    uint16_t      n_register;

    neu_dtype_e               type;
    neu_datatag_addr_option_u option;
    char                      name[NEU_TAG_NAME_LEN];
} modbus_point_t;

int modbus_tag_to_point(neu_datatag_t *tag, modbus_point_t *point);

typedef struct modbus_read_cmd {
    uint8_t       slave_id;
    modbus_area_e area;
    uint16_t      start_address;
    uint16_t      n_register;

    UT_array *tags; // modbus_point_t ptr;
} modbus_read_cmd_t;

typedef struct modbus_read_cmd_sort {
    uint16_t           n_cmd;
    modbus_read_cmd_t *cmd;
} modbus_read_cmd_sort_t;

modbus_read_cmd_sort_t *modbus_tag_sort(UT_array *tags, uint16_t max_byte);
void                    modbus_tag_sort_free(modbus_read_cmd_sort_t *cs);

#ifdef __cplusplus
}
#endif

#endif