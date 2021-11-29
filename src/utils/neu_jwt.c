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
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include "config.h"
#include "neu_errcodes.h"
#include "neu_jwt.h"
#include "neu_log.h"

char   public_key[4096]  = { 0 };
char   private_key[4096] = { 0 };
char   token_local[4096] = { 0 };
size_t priv_key_len      = 0;
size_t pub_key_len       = 0;

int neu_jwt_init(char *pub_key_path, char *priv_key_path)
{
    FILE *fp_priv_key = NULL;
    FILE *fp_pub_key  = NULL;

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
    struct timeval tv      = { 0 };
    jwt_alg_t      opt_alg = JWT_ALG_RS256;
    jwt_t *        jwt     = NULL;
    char *         str_jwt = NULL;

    int ret = jwt_new(&jwt);
    if (ret != 0 || jwt == NULL) {
        log_error("Invalid jwt: %d, errno: %d", ret, errno);
        return -1;
    }

    gettimeofday(&tv, NULL);

    jwt_add_grant_int(jwt, "exp", tv.tv_sec + 60 * 60);
    if (ret != 0) {
        jwt_free(jwt);
        log_error("Failed to add exp: %d, errno: %d", ret, errno);
        return -1;
    }

    ret = jwt_set_alg(jwt, opt_alg, (const unsigned char *) private_key,
                      priv_key_len);
    if (ret != 0) {
        jwt_free(jwt);
        log_error("jwt incorrect algorithm: %d, errno: %d", ret, errno);
        return -1;
    }

    str_jwt = jwt_encode_str(jwt);
    if (str_jwt == NULL) {
        jwt_free(jwt);
        log_error("jwt incorrect algorithm, errno: %d", errno);
        return -1;
    }

    *token = str_jwt;
    strncpy(token_local, *token, strlen(*token));

    jwt_free(jwt);

    return 0;
}

static void *neu_jwt_decode(char *token)
{
    jwt_t *jwt = NULL;
    int    ret = jwt_decode(&jwt, token, (const unsigned char *) public_key,
                         pub_key_len);

    if (ret != 0) {
        log_error("jwt decode error: %d", ret);
        return NULL;
    }

    if (jwt_get_alg(jwt) != JWT_ALG_RS256) {
        log_error("jwt decode alg: %d", jwt_get_alg(jwt));
        jwt_free(jwt);
        return NULL;
    }

    return jwt;
}

int neu_jwt_validate(char *b_token)
{
    jwt_valid_t *jwt_valid = NULL;
    jwt_alg_t    opt_alg   = JWT_ALG_RS256;
    char *       token     = NULL;

    if (b_token == NULL || strlen(b_token) <= strlen("Bearar ")) {
        return NEU_ERR_NEED_TOKEN;
    }

    token = &b_token[strlen("Bearar ")];

    jwt_t *jwt = (jwt_t *) neu_jwt_decode(token);

    if (jwt == NULL) {
        return NEU_ERR_DECODE_TOKEN;
    }

    int ret = jwt_valid_new(&jwt_valid, opt_alg);
    if (ret != 0 || jwt_valid == NULL) {
        jwt_valid_free(jwt_valid);
        jwt_free(jwt);
        log_error("Failed to allocate jwt_valid: %d", ret);
        return NEU_ERR_EINTERNAL;
    }

    ret = jwt_valid_set_headers(jwt_valid, 1);
    if (ret != 0 || jwt_valid == NULL) {
        jwt_valid_free(jwt_valid);
        jwt_free(jwt);
        log_error("Failed to set jwt headers: %d", ret);
        return NEU_ERR_EINTERNAL;
    }

    ret = jwt_valid_set_now(jwt_valid, time(NULL));
    if (ret != 0 || jwt_valid == NULL) {
        jwt_valid_free(jwt_valid);
        jwt_free(jwt);
        log_error("Failed to set time: %d", ret);
        return NEU_ERR_EINTERNAL;
    }

    ret = jwt_validate(jwt, jwt_valid);
    if (ret != 0) {
        jwt_valid_free(jwt_valid);
        jwt_free(jwt);
        log_error("Jwt failed to validate : %d", ret);
        if (ret == JWT_VALIDATION_EXPIRED) {
            return NEU_ERR_EXPIRED_TOKEN;
        } else {
            return NEU_ERR_VALIDATE_TOKEN;
        }
    }

    jwt_valid_free(jwt_valid);
    jwt_free(jwt);

    if (strcmp(token_local, token) != 0) {
        return NEU_ERR_INVALID_TOKEN;
    }

    return NEU_ERR_SUCCESS;
}

void neu_jwt_destroy()
{
    memset(token_local, 0, sizeof(token_local));
}