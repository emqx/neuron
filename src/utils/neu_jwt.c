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

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include <jwt.h>

#include "errcodes.h"
#include "utils/log.h"
#include "utils/neu_jwt.h"

struct public_key_store {
    struct {
        char name[257];
        char key[1024];
    } key[16];
    int size;
};

static struct public_key_store key_store;
static char                    neuron_private_key[2048] = { 0 };
static char                    neuron_public_key[2048]  = { 0 };

static int find_key(const char *name)
{
    for (int i = 0; i < key_store.size; i++) {
        if (strncmp(name, key_store.key[i].name, strlen(name)) == 0) {
            return i;
        }
    }

    return -1;
}

static void add_key(char *name, char *value)
{
    key_store.size += 1;
    assert(key_store.size <= 8);

    strncpy(key_store.key[key_store.size - 1].name, name,
            sizeof(key_store.key[key_store.size - 1].name) - 1);
    strncpy(key_store.key[key_store.size - 1].key, value,
            sizeof(key_store.key[key_store.size - 1].key) - 1);
}

static char *load_key(const char *dir, char *name)
{
    FILE *      f             = NULL;
    int         len           = 0;
    char        path[256]     = { 0 };
    static char content[2047] = { 0 };

    snprintf((char *) path, sizeof(path), "%s/%s", dir, name);
    memset(content, 0, sizeof(content));

    f = fopen(path, "r");
    if (f == NULL) {
        zlog_error(neuron, "Failed to open file: %s, errno: %d", name, errno);
        return NULL;
    }

    len = fread(content, 1, sizeof(content), f);
    if (len <= 0) {
        zlog_error(neuron, "Failed to read  file: %s, errno: %d", name, errno);

        fclose(f);
        return NULL;
    }

    fclose(f);
    return content;
}

static void load_neuron_key(const char *dir_path)
{
    char *content = load_key(dir_path, "neuron.key");
    assert(content != NULL);

    strncpy(neuron_private_key, content, sizeof(neuron_private_key) - 1);

    content = load_key(dir_path, "neuron.pem");
    assert(content != NULL);

    strncpy(neuron_public_key, content, sizeof(neuron_public_key) - 1);
}

static void scanf_key(const char *dir_path)
{
    DIR *          dir = NULL;
    struct dirent *ptr = NULL;

    memset(&key_store, 0, sizeof(key_store));

    dir = opendir(dir_path);
    if (dir == NULL) {
        zlog_error(neuron, "Open dir error: %s", strerror(errno));
        return;
    }

    while (NULL != (ptr = readdir(dir))) {
        char *content = NULL;

        if (!strcmp((char *) ptr->d_name, ".") ||
            !strcmp((char *) ptr->d_name, "..")) {
            continue;
        }

        if (strstr((char *) ptr->d_name, ".pem") != NULL ||
            strstr((char *) ptr->d_name, ".pub") != NULL) {
            content = load_key(dir_path, (char *) ptr->d_name);
            assert(content != NULL);
            add_key((char *) ptr->d_name, content);
        }
    }

    closedir(dir);
}

int neu_jwt_init(const char *dir_path)
{
    load_neuron_key(dir_path);
    scanf_key("certs");
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
        zlog_error(neuron, "Invalid jwt: %d, errno: %d", ret, errno);
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

    ret = jwt_add_grant_int(jwt, "exp", tv.tv_sec + 24 * 60 * 60);
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

    ret = jwt_set_alg(jwt, opt_alg, (const unsigned char *) neuron_private_key,
                      strlen(neuron_private_key));
    if (ret != 0) {
        jwt_free(jwt);
        zlog_error(neuron, "jwt incorrect algorithm: %d, errno: %d", ret,
                   errno);
        return -1;
    }

    str_jwt = jwt_encode_str(jwt);
    if (str_jwt == NULL) {
        jwt_free(jwt);
        zlog_error(neuron, "jwt incorrect algorithm, errno: %d", errno);
        return -1;
    }

    *token = str_jwt;

    jwt_free(jwt);

    return 0;

err_out:
    zlog_error(neuron, "Failed to add a grant: %d, errno: %d", ret, errno);

    jwt_free(jwt);
    return -1;
}

static void *neu_jwt_decode(char *token)
{
    jwt_t *     jwt      = NULL;
    jwt_t *     jwt_test = NULL;
    int         ret      = -1;
    const char *name     = NULL;

    ret = jwt_decode(&jwt_test, token, NULL, 0);
    if (ret != 0) {
        zlog_error(neuron, "jwt decode error: %d", ret);
        return NULL;
    }

    name = jwt_get_grant(jwt_test, "iss");
    if (NULL == name) {
        zlog_error(neuron, "jwt get iss grant error, the token is: %s", token);
        jwt_free(jwt_test);
        return NULL;
    }

    ret = find_key(name);
    if (ret >= 0) {
        ret = jwt_decode(&jwt, token,
                         (const unsigned char *) key_store.key[ret].key,
                         strlen(key_store.key[ret].key));
        if (ret != 0) {
            zlog_error(neuron, "jwt decode error: %d", ret);
            jwt_free(jwt_test);
            jwt_free(jwt);
            return NULL;
        }
    } else if (strcmp(name, "neuron") == 0) {
        ret = jwt_decode(&jwt, token, (const unsigned char *) neuron_public_key,
                         strlen(neuron_public_key));
        if (ret != 0) {
            zlog_error(neuron, "jwt decode error: %d", ret);
            jwt_free(jwt_test);
            jwt_free(jwt);
            return NULL;
        }
    } else {
        zlog_error(neuron, "Don't find public key file: %s", name);
        jwt_free(jwt_test);
        jwt_free(jwt);
        scanf_key("certs");
        return NULL;
    }

    if (jwt_get_alg(jwt) != JWT_ALG_RS256) {
        zlog_error(neuron, "jwt decode alg: %d", jwt_get_alg(jwt));
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
        zlog_error(neuron, "Failed to allocate jwt_valid: %d", ret);
        jwt_valid_free(jwt_valid);
        jwt_free(jwt);
        return NEU_ERR_EINTERNAL;
    }

    ret = jwt_valid_set_headers(jwt_valid, 1);
    if (ret != 0 || jwt_valid == NULL) {
        zlog_error(neuron, "Failed to set jwt headers: %d", ret);
        jwt_valid_free(jwt_valid);
        jwt_free(jwt);
        return NEU_ERR_EINTERNAL;
    }

    ret = jwt_valid_set_now(jwt_valid, time(NULL));
    if (ret != 0 || jwt_valid == NULL) {
        zlog_error(neuron, "Failed to set time: %d", ret);
        jwt_valid_free(jwt_valid);
        jwt_free(jwt);
        return NEU_ERR_EINTERNAL;
    }

    ret = jwt_validate(jwt, jwt_valid);
    if (ret != 0) {
        zlog_error(neuron, "Jwt failed to validate : %d", ret);
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
