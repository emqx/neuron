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

#ifndef _NEU_JSON_API_DRIVER_CMD_H_
#define _NEU_JSON_API_DRIVER_CMD_H_

#include "define.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char *driver;
    char *action;
} neu_json_driver_action_t;

int  neu_json_decode_driver_action_req(char *                     buf,
                                       neu_json_driver_action_t **result);
void neu_json_decode_driver_action_req_free(neu_json_driver_action_t *req);

typedef struct {
    char *driver;
    char *path;
} neu_json_driver_directory_req_t;

int neu_json_decode_driver_directory_req(
    char *buf, neu_json_driver_directory_req_t **result);
void neu_json_decode_driver_directory_req_free(
    neu_json_driver_directory_req_t *req);

typedef struct {
    char *  name;
    int     ftype;
    int64_t size;
    int64_t timestamp;
} neu_json_driver_directory_file_t;

typedef struct {
    int                               error;
    neu_json_driver_directory_file_t *files;
    int                               n_files;
} neu_json_driver_directory_resp_t;

int neu_json_encode_driver_directory_resp(void *json_object, void *param);

typedef struct {
    char *driver;
    char *path;
} neu_json_driver_fup_open_req_t;

int neu_json_decode_driver_fup_open_req(
    char *buf, neu_json_driver_fup_open_req_t **result);
void neu_json_decode_driver_fup_open_req_free(
    neu_json_driver_fup_open_req_t *req);

typedef struct {
    int     error;
    int64_t size;
} neu_json_driver_fup_open_resp_t;

int neu_json_encode_driver_fup_open_resp(void *json_object, void *param);

typedef struct {
    char *driver;
    char *src_path;
    char *dst_path;
} neu_json_driver_fdown_open_req_t;

int neu_json_decode_driver_fdown_open_req(
    char *buf, neu_json_driver_fdown_open_req_t **result);
void neu_json_decode_driver_fdown_open_req_free(
    neu_json_driver_fdown_open_req_t *req);

typedef struct {
    int error;
} neu_json_driver_fdown_open_resp_t;

int neu_json_encode_driver_fdown_open_resp(void *json_object, void *param);

typedef struct {
    char *driver;
    char *path;
} neu_json_driver_fup_data_req_t;

int neu_json_decode_driver_fup_data_req(
    char *buf, neu_json_driver_fup_data_req_t **result);
void neu_json_decode_driver_fup_data_req_free(
    neu_json_driver_fup_data_req_t *req);

typedef struct {
    int      error;
    bool     more;
    uint8_t *data;
    uint16_t len;
} neu_json_driver_fup_data_resp_t;

int neu_json_encode_driver_fup_data_resp(void *json_object, void *param);

#ifdef __cplusplus
}
#endif

#endif