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

#ifndef NEURON_REST_USER_H
#define NEURON_REST_USER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#include "persist/persist.h"

typedef struct {
    char *name;
    char *hash;
} neu_user_t;

neu_user_t *neu_user_new(const char *name, const char *password);
void        neu_user_free(neu_user_t *user);
bool        neu_user_check_password(neu_user_t *user, const char *password);
int neu_user_update_password(neu_user_t *user, const char *new_password);

neu_user_t *neu_load_user(const char *name);
int         neu_save_user(neu_user_t *user);

char *neu_decrypt_user_password(const char *crypted_password);

#ifdef __cplusplus
}
#endif

#endif
