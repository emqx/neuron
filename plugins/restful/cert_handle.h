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

#ifndef _NEU_PLUGIN_CERT_HANDLE_H_
#define _NEU_PLUGIN_CERT_HANDLE_H_

#include <nng/nng.h>

#include "rest.h"

void handle_server_cert_self_sign(nng_aio *aio);
void handle_server_cert_upload(nng_aio *aio);
void handle_server_cert_get(nng_aio *aio);
void handle_server_cert_get_resp(nng_aio *                    aio,
                                 neu_resp_server_cert_data_t *resp);
void handle_server_cert_export(nng_aio *aio);
void handle_server_cert_export_resp(nng_aio *                      aio,
                                    neu_resp_server_cert_export_t *resp);
void handle_client_cert_upload(nng_aio *aio);
void handle_client_cert_delete(nng_aio *aio);
void handle_client_cert_trust(nng_aio *aio);
void handle_client_cert_get(nng_aio *aio);
void handle_client_cert_get_resp(nng_aio *                     aio,
                                 neu_resp_client_certs_data_t *resp);

// Authentication related functions
void handle_auth_basic_enable(nng_aio *aio);
void handle_auth_basic_status(nng_aio *aio);
void handle_auth_basic_status_resp(nng_aio *                             aio,
                                   neu_resp_server_auth_switch_status_t *resp);
void handle_add_basic_user(nng_aio *aio);
void handle_update_basic_user(nng_aio *aio);
void handle_delete_basic_user(nng_aio *aio);
void handle_get_basic_users(nng_aio *aio);
void handle_get_basic_users_resp(nng_aio *                          aio,
                                 neu_resp_server_auth_users_info_t *resp);

// Security policy related functions
void handle_server_security_policy(nng_aio *aio);
void handle_get_server_security_policy(nng_aio *aio);
void handle_get_server_security_policy_resp(
    nng_aio *aio, neu_resp_server_security_policy_status_t *resp);

#endif
