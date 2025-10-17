/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2024 EMQ Technologies Co., Ltd All rights reserved.
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

#ifndef _NEU_JSON_API_NEU_JSON_CERT_H_
#define _NEU_JSON_API_NEU_JSON_CERT_H_

#include "define.h"
#include "json/json.h"

#ifdef __cplusplus
extern "C" {
#endif

// Request structures for certificate operations
typedef struct {
    char *app_name;
    int   days_valid;
} neu_json_server_cert_self_sign_req_t;

typedef struct {
    char *app_name;
    char *certificate;
    char *private_key;
} neu_json_server_cert_upload_req_t;

typedef struct {
    char *app_name;
    char *fingerprint;
} neu_json_client_cert_delete_req_t;

typedef struct {
    char *app_name;
    char *fingerprint;
} neu_json_client_cert_trust_req_t;

typedef struct {
    char *app_name;
    char *certificate;
    int   is_ca;
} neu_json_client_cert_upload_req_t;

// Response structures for certificate operations
typedef struct {
    char *subject;
    char *fingerprint;
    char *issuer;
    char *valid_to;
} neu_json_server_cert_info_resp_t;

typedef struct {
    char *subject;
    char *fingerprint;
    char *issuer;
    char *valid_to;
    int   trust_status;
    int   ca;
} neu_json_client_cert_info_t;

typedef struct {
    neu_json_client_cert_info_t **certs;
    int                           count;
} neu_json_client_cert_info_resp_t;

// Security policy related structures
typedef struct {
    char *   app_name;
    char **  policy_names;
    uint16_t policy_count;
} neu_json_server_security_policy_req_t;

typedef struct {
    char *app_name;
} neu_json_get_security_policy_req_t;

typedef struct {
    char **  policy_names;
    uint16_t policy_count;
} neu_json_get_security_policy_resp_t;

// Authentication related structures
typedef struct {
    char *app_name;
    bool  enabled;
} neu_json_auth_basic_enable_req_t;

typedef struct {
    char *app_name;
    char *username;
    char *password;
} neu_json_add_basic_user_req_t;

typedef struct {
    char *app_name;
    char *username;
    char *new_password;
} neu_json_update_basic_user_req_t;

typedef struct {
    char *app_name;
    char *username;
} neu_json_delete_basic_user_req_t;

typedef struct {
    char *app_name;
} neu_json_get_basic_users_req_t;

typedef struct {
    char *username;
    char *password_masked;
} neu_json_basic_user_info_t;

typedef struct {
    neu_json_basic_user_info_t **users;
    int                          count;
} neu_json_get_basic_users_resp_t;

typedef struct {
    char *app_name;
} neu_json_auth_basic_status_req_t;

typedef struct {
    bool enabled;
} neu_json_auth_basic_status_resp_t;

// Function declarations
int neu_json_decode_server_cert_self_sign_req(
    char *buf, neu_json_server_cert_self_sign_req_t **result);
void neu_json_decode_server_cert_self_sign_req_free(
    neu_json_server_cert_self_sign_req_t *req);

int neu_json_decode_server_cert_upload_req(
    char *buf, neu_json_server_cert_upload_req_t **result);
void neu_json_decode_server_cert_upload_req_free(
    neu_json_server_cert_upload_req_t *req);

int neu_json_decode_client_cert_delete_req(
    char *buf, neu_json_client_cert_delete_req_t **result);
void neu_json_decode_client_cert_delete_req_free(
    neu_json_client_cert_delete_req_t *req);

int neu_json_decode_client_cert_trust_req(
    char *buf, neu_json_client_cert_trust_req_t **result);
void neu_json_decode_client_cert_trust_req_free(
    neu_json_client_cert_trust_req_t *req);

int neu_json_decode_client_cert_upload_req(
    char *buf, neu_json_client_cert_upload_req_t **result);
void neu_json_decode_client_cert_upload_req_free(
    neu_json_client_cert_upload_req_t *req);

int neu_json_encode_server_cert_info_resp(void *json_obj, void *param);
int neu_json_encode_client_cert_info_resp(void **json_obj, void *param);

// Security policy related functions
int neu_json_decode_server_security_policy_req(
    char *buf, neu_json_server_security_policy_req_t **result);
void neu_json_decode_server_security_policy_req_free(
    neu_json_server_security_policy_req_t *req);

int neu_json_decode_get_security_policy_req(
    char *buf, neu_json_get_security_policy_req_t **result);
void neu_json_decode_get_security_policy_req_free(
    neu_json_get_security_policy_req_t *req);

int neu_json_encode_get_security_policy_resp(void *json_obj, void *param);

// Authentication related functions
int neu_json_decode_auth_basic_enable_req(
    char *buf, neu_json_auth_basic_enable_req_t **result);
void neu_json_decode_auth_basic_enable_req_free(
    neu_json_auth_basic_enable_req_t *req);

int  neu_json_decode_add_basic_user_req(char *                          buf,
                                        neu_json_add_basic_user_req_t **result);
void neu_json_decode_add_basic_user_req_free(
    neu_json_add_basic_user_req_t *req);

int neu_json_decode_update_basic_user_req(
    char *buf, neu_json_update_basic_user_req_t **result);
void neu_json_decode_update_basic_user_req_free(
    neu_json_update_basic_user_req_t *req);

int neu_json_decode_delete_basic_user_req(
    char *buf, neu_json_delete_basic_user_req_t **result);
void neu_json_decode_delete_basic_user_req_free(
    neu_json_delete_basic_user_req_t *req);

int neu_json_decode_get_basic_users_req(
    char *buf, neu_json_get_basic_users_req_t **result);
void neu_json_decode_get_basic_users_req_free(
    neu_json_get_basic_users_req_t *req);

int neu_json_encode_get_basic_users_resp(void *json_obj, void *param);

int neu_json_decode_auth_basic_status_req(
    char *buf, neu_json_auth_basic_status_req_t **result);
void neu_json_decode_auth_basic_status_req_free(
    neu_json_auth_basic_status_req_t *req);

int neu_json_encode_auth_basic_status_resp(void *json_obj, void *param);

#ifdef __cplusplus
}
#endif

#endif
