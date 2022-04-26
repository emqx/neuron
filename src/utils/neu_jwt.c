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

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "config.h"
#include "errcodes.h"
#include "log.h"
#include "neu_jwt.h"

typedef struct {
    char   name[256];
    char   key[4096];
    size_t len;
} public_key_t;

int n_pub_file = 0;

public_key_t pub_key_files[8];

size_t priv_key_len      = 0;
char   private_key[4096] = { 0 };

static int handle_public_key(char *dir_path, char *public_name)
{
    FILE *fp_pub_key    = NULL;
    char  pub_path[512] = { 0 };

    snprintf((char *) pub_path, sizeof(pub_path), "%s/%s", dir_path,
             public_name);

    fp_pub_key = fopen(pub_path, "r");
    if (fp_pub_key == NULL) {
        log_error("Failed to open public key file: %s, errno: %d", public_name,
                  errno);
        return -1;
    }

    pub_key_files[n_pub_file].len =
        fread(pub_key_files[n_pub_file].key, 1,
              sizeof(pub_key_files[n_pub_file].key), fp_pub_key);
    if (pub_key_files[n_pub_file].len <= 0) {
        log_error("Failed to read public key file: %s, errno: %d", public_name,
                  errno);

        fclose(fp_pub_key);
        return -1;
    }

    strncpy(pub_key_files[n_pub_file].name, public_name,
            sizeof(pub_key_files[n_pub_file].name) - 1);

    n_pub_file += 1;

    fclose(fp_pub_key);
    return 0;
}

static int handle_private_key(char *dir_path, char *private_name)
{
    FILE *fp_priv_key     = NULL;
    char  priv_path[1024] = { 0 };

    snprintf((char *) priv_path, sizeof(priv_path), "%s/%s", dir_path,
             private_name);

    fp_priv_key = fopen(priv_path, "r");
    if (fp_priv_key == NULL) {
        log_error("Failed to open private key file: %s, errno: %d",
                  private_name, errno);
        return -1;
    }

    priv_key_len = fread(private_key, 1, sizeof(private_key), fp_priv_key);
    if (priv_key_len <= 0) {
        log_error("Failed to read key file: %s, error: %d", priv_path, errno);

        fclose(fp_priv_key);
        return -1;
    }

    fclose(fp_priv_key);
    return 0;
}

int neu_jwt_init(char *dir_path)
{
    DIR *          dir = NULL;
    struct dirent *ptr = NULL;
    int            ret = -1;

    dir = opendir(dir_path);
    if (dir == NULL) {
        log_error("Open dir error: %s", strerror(errno));
        return -1;
    }

    while (NULL != (ptr = readdir(dir))) {
        if (!strcmp((char *) ptr->d_name, ".") ||
            !strcmp((char *) ptr->d_name, "..") ||
            !strcmp((char *) ptr->d_name, "neuron.yaml")) {
            continue;
        }

        if (strstr((char *) ptr->d_name, ".key") != NULL) {
            ret = handle_private_key(dir_path, (char *) ptr->d_name);
            if (ret != 0) {
                closedir(dir);
                return ret;
            }
        }

        if (strstr((char *) ptr->d_name, ".pem") != NULL) {
            ret = handle_public_key(dir_path, (char *) ptr->d_name);
            if (ret != 0) {
                closedir(dir);
                return ret;
            }
        }
    }

    if (n_pub_file == 0) {
        log_error("Don't find key files");

        closedir(dir);
        return -1;
    }

    closedir(dir);
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

    ret = jwt_add_grant(jwt, "iss", "neuron");
    if (ret != 0) {
        goto err_out;
    }

    ret = jwt_add_grant_int(jwt, "iat", tv.tv_sec);
    if (ret != 0) {
        goto err_out;
    }

    ret = jwt_add_grant_int(jwt, "exp", tv.tv_sec + 60 * 60);
    if (ret != 0) {
        goto err_out;
    }

    ret = jwt_add_grant(jwt, "aud", "neuron");
    if (ret != 0) {
        goto err_out;
    }

    ret = jwt_add_grant_int(jwt, "bodyEncode", 0);
    if (ret != 0) {
        goto err_out;
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

    jwt_free(jwt);

    return 0;

err_out:
    log_error("Failed to add a grant: %d, errno: %d", ret, errno);

    jwt_free(jwt);
    return -1;
}

static void *neu_jwt_decode(char *token)
{
    jwt_t *     jwt      = NULL;
    jwt_t *     jwt_test = NULL;
    int         ret      = -1;
    int         count    = 0;
    const char *name     = NULL;

    ret = jwt_decode(&jwt_test, token, NULL, 0);
    if (ret != 0) {
        log_error("jwt decode error: %d", ret);
        return NULL;
    }

    name = jwt_get_grant(jwt_test, "iss");
    if (NULL == name) {
        log_error("jwt get iss grant error, the token is: %s", token);
        jwt_free(jwt_test);
        return NULL;
    }

    for (int i = 0; i < n_pub_file; i++) {
        if (NULL != strstr((char *) pub_key_files[i].name, name)) {
            ret = jwt_decode(&jwt, token,
                             (const unsigned char *) pub_key_files[i].key,
                             pub_key_files[i].len);
            if (ret != 0) {
                log_error("jwt decode error: %d", ret);
                jwt_free(jwt_test);
                jwt_free(jwt);
                return NULL;
            }
        } else {
            count += 1;

            if (count == n_pub_file) {
                log_error("Don't find public key file: %s", name);
                jwt_free(jwt_test);
                jwt_free(jwt);
                return NULL;
            }
        }
    }

    if (jwt_get_alg(jwt) != JWT_ALG_RS256) {
        log_error("jwt decode alg: %d", jwt_get_alg(jwt));
        jwt_free(jwt_test);
        jwt_free(jwt);
        return NULL;
    }

    jwt_free(jwt_test);
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
        log_error("Failed to allocate jwt_valid: %d", ret);
        jwt_valid_free(jwt_valid);
        jwt_free(jwt);
        return NEU_ERR_EINTERNAL;
    }

    ret = jwt_valid_set_headers(jwt_valid, 1);
    if (ret != 0 || jwt_valid == NULL) {
        log_error("Failed to set jwt headers: %d", ret);
        jwt_valid_free(jwt_valid);
        jwt_free(jwt);
        return NEU_ERR_EINTERNAL;
    }

    ret = jwt_valid_set_now(jwt_valid, time(NULL));
    if (ret != 0 || jwt_valid == NULL) {
        log_error("Failed to set time: %d", ret);
        jwt_valid_free(jwt_valid);
        jwt_free(jwt);
        return NEU_ERR_EINTERNAL;
    }

    ret = jwt_validate(jwt, jwt_valid);
    if (ret != 0) {
        log_error("Jwt failed to validate : %d", ret);
        jwt_valid_free(jwt_valid);
        jwt_free(jwt);
        if (ret == JWT_VALIDATION_EXPIRED) {
            return NEU_ERR_EXPIRED_TOKEN;
        } else {
            return NEU_ERR_VALIDATE_TOKEN;
        }
    }

    jwt_valid_free(jwt_valid);
    jwt_free(jwt);

    return NEU_ERR_SUCCESS;
}

void neu_jwt_destroy()
{
    // memset(token_local, 0, sizeof(token_local));
}