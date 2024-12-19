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
#ifndef __NEU_JWT_H__
#define __NEU_JWT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

int  neu_jwt_init(const char *dir_path);
int  neu_jwt_new(char **token, const char *user);
int  neu_jwt_validate(char *b_token);
void neu_jwt_destroy();
void neu_jwt_decode_user_after_valid(char *bearer, char *user);

#ifdef __cplusplus
}
#endif

#endif