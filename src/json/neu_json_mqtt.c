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
#include "utils/neu_json.h"

#include "neu_json_mqtt.h"

int neu_json_decode_mqtt_param_req(char *buf, neu_json_mqtt_param_t *param)
{

    neu_data_val_t *val =
        neu_dvalue_array_new(NEU_DTYPE_DATA_VAL, 2, sizeof(neu_data_val_t *));
    neu_data_val_t *function =
        neu_dvalue_array_new(NEU_DTYPE_DATA_VAL, 2, sizeof(neu_data_val_t *));
    neu_data_val_t *uuid =
        neu_dvalue_array_new(NEU_DTYPE_DATA_VAL, 2, sizeof(neu_data_val_t *));
    neu_fixed_array_t *val_array =
        neu_fixed_array_new(2, sizeof(neu_data_val_t *));
    neu_fixed_array_t *function_array =
        neu_fixed_array_new(2, sizeof(neu_data_val_t *));
    neu_fixed_array_t *uuid_array =
        neu_fixed_array_new(2, sizeof(neu_data_val_t *));

    // todo add name to array
    neu_fixed_array_set(function_array, 1, param->function);
    neu_dvalue_set_array(function, function_array);

    neu_fixed_array_set(uuid_array, 1, param->uuid);
    neu_dvalue_set_array(uuid, uuid_array);

    neu_fixed_array_set(val_array, 0, function);
    neu_fixed_array_set(val_array, 1, uuid);
    neu_dvalue_set_array(val, val_array);

    return neu_jsonx_decode(buf, val);
}

int neu_json_encode_mqtt_param_res(void *                 json_object,
                                   neu_json_mqtt_param_t *param)
{
    neu_fixed_array_t *function_array =
        neu_fixed_array_new(2, sizeof(neu_data_val_t *));
    neu_fixed_array_t *uuid_array =
        neu_fixed_array_new(2, sizeof(neu_data_val_t *));

    // todo add name to array
    neu_fixed_array_set(function_array, 1, param->function);
    neu_fixed_array_set(uuid_array, 1, param->uuid);

    neu_jsonx_encode_field(json_object, function_array);
    neu_jsonx_encode_field(json_object, uuid_array);

    return 0;
}