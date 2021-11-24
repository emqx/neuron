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

#include <errno.h>

#include "config.h"
#include "neu_jwt.h"
#include "neu_log.h"

unsigned char public_key[4096]  = { 0 };
unsigned char private_key[4096] = { 0 };
size_t        priv_key_len      = 0;
size_t        pub_key_len       = 0;

int neu_jwt_init(char *pub_key_path, char *priv_key_path)
{
    FILE * fp_priv_key  = NULL;
    FILE * fp_pub_key   = NULL;
    size_t priv_key_len = 0;
    size_t pub_key_len  = 0;

    fp_priv_key = fopen(priv_key_path, "r");
    if (fp_priv_key == NULL) {
        log_error("Failed to open key file: %s, error: %d", priv_key_path,
                  errno);
        return -1;
    }
    priv_key_len = fread(private_key, 1, sizeof(private_key), fp_priv_key);
    if (priv_key_len <= 0) {
        log_error("Failed to read key file: %s, error: %d", priv_key_path,
                  errno);
        return -1;
    }
    fclose(fp_priv_key);

    fp_pub_key = fopen(pub_key_path, "r");
    if (fp_pub_key == NULL) {
        log_error("Failed to open key file: %s, error: %d", pub_key_path,
                  errno);
        return -1;
    }
    pub_key_len = fread(public_key, 1, sizeof(public_key), fp_pub_key);
    if (pub_key_len <= 0) {
        log_error("Failed to read key file: %s, error: %d", pub_key_path,
                  errno);
        return -1;
    }
    fclose(fp_pub_key);

    return 0;
}

int neu_jwt_new(char **token)
{
    time_t    iat     = time(NULL);
    jwt_alg_t opt_alg = JWT_ALG_RS256;
    jwt_t *   jwt     = NULL;
    char *    str_jwt = NULL;

    int ret = jwt_new(&jwt);
    if (ret != 0 || jwt == NULL) {
        log_error("Invalid jwt: %d", ret);
        return -1;
    }
    ret = jwt_add_grant_int(jwt, "iat", iat);
    if (ret < 0) {
        jwt_free(jwt);
        log_error("Failed to add grant: %d", ret);
        return -1;
    }

    ret = jwt_set_alg(jwt, opt_alg, private_key, priv_key_len);
    if (ret < 0) {
        jwt_free(jwt);
        log_error("jwt incorrect algorithm: %d", ret);
        return -1;
    }

    str_jwt = jwt_encode_str(jwt);
    if (str_jwt == NULL) {
        jwt_free(jwt);
        log_error("jwt incorrect algorithm");
        return -1;
    }
    *token = str_jwt;

    jwt_free(jwt);
    jwt_free_str(str_jwt);

    return 0;
}

// int neu_jwt_validate(char *token)
// {
//     // jwt_valid_t *jwt_valid = NULL;
//     (void) token;

//     return 0;
// }