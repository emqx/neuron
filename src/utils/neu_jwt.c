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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include <openssl/evp.h>
#include <openssl/pem.h>

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

bool disable_jwt = false;

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

static bool load_key_file(const char *dir, const char *name, char *dst,
                          size_t dst_len)
{
    FILE *      f             = NULL;
    int         len           = 0;
    char        path[256]     = { 0 };
    static char content[2047] = { 0 };

    snprintf((char *) path, sizeof(path), "%s/%s", dir, name);
    memset(content, 0, sizeof(content));

    f = fopen(path, "r");
    if (f == NULL) {
        return false;
    }

    len = fread(content, 1, sizeof(content), f);
    if (len <= 0) {
        fclose(f);
        return false;
    }

    fclose(f);

    strncpy(dst, content, dst_len - 1);
    dst[dst_len - 1] = '\0';
    return true;
}

static bool write_private_key(FILE *file, EVP_PKEY *pkey)
{
    if (fchmod(fileno(file), S_IRUSR | S_IWUSR) != 0) {
        return false;
    }

    return PEM_write_PrivateKey(file, pkey, NULL, NULL, 0, NULL, NULL) == 1;
}

static bool write_public_key(FILE *file, EVP_PKEY *pkey)
{
    if (fchmod(fileno(file), S_IRUSR | S_IWUSR) != 0) {
        return false;
    }

    return PEM_write_PUBKEY(file, pkey) == 1;
}

static bool generate_key_pair(const char *dir_path)
{
    EVP_PKEY_CTX *ctx               = NULL;
    EVP_PKEY *    pkey              = NULL;
    FILE *        private_file      = NULL;
    FILE *        public_file       = NULL;
    char          private_path[256] = { 0 };
    char          public_path[256]  = { 0 };
    bool          ok                = false;

    snprintf(private_path, sizeof(private_path), "%s/%s", dir_path,
             "neuron.key");
    snprintf(public_path, sizeof(public_path), "%s/%s", dir_path,
             "neuron.key.pub");

    ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
    if (ctx == NULL) {
        return false;
    }

    if (EVP_PKEY_keygen_init(ctx) <= 0 ||
        EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048) <= 0 ||
        EVP_PKEY_keygen(ctx, &pkey) <= 0) {
        goto done;
    }

    private_file = fopen(private_path, "w");
    if (private_file == NULL) {
        goto done;
    }

    public_file = fopen(public_path, "w");
    if (public_file == NULL) {
        goto done;
    }

    ok = write_private_key(private_file, pkey) &&
        write_public_key(public_file, pkey);

done:
    if (private_file != NULL) {
        fclose(private_file);
    }
    if (public_file != NULL) {
        fclose(public_file);
    }
    EVP_PKEY_free(pkey);
    EVP_PKEY_CTX_free(ctx);

    if (!ok) {
        unlink(private_path);
        unlink(public_path);
    }

    return ok;
}

static void load_neuron_key(const char *dir_path)
{
    char private_path[256] = { 0 };
    char public_path[256]  = { 0 };

    if (dir_path == NULL || *dir_path == '\0') {
        zlog_error(neuron, "JWT config directory is empty");
        return;
    }

    snprintf(private_path, sizeof(private_path), "%s/%s", dir_path,
             "neuron.key");
    snprintf(public_path, sizeof(public_path), "%s/%s", dir_path,
             "neuron.key.pub");

    if (!load_key_file(dir_path, "neuron.key", neuron_private_key,
                       sizeof(neuron_private_key)) ||
        !load_key_file(dir_path, "neuron.key.pub", neuron_public_key,
                       sizeof(neuron_public_key))) {
        zlog_warn(neuron, "Missing JWT RSA key pair in %s, generating new keys",
                  dir_path);
        if (!generate_key_pair(dir_path)) {
            zlog_error(neuron, "Failed to generate JWT RSA key pair in %s",
                       dir_path);
            neuron_private_key[0] = '\0';
            neuron_public_key[0]  = '\0';
            return;
        }

        zlog_notice(neuron,
                    "Generated JWT RSA key pair in config directory: %s",
                    dir_path);

        if (!load_key_file(dir_path, "neuron.key", neuron_private_key,
                           sizeof(neuron_private_key)) ||
            !load_key_file(dir_path, "neuron.key.pub", neuron_public_key,
                           sizeof(neuron_public_key))) {
            zlog_error(neuron,
                       "Failed to load generated JWT RSA key pair from %s",
                       dir_path);
            neuron_private_key[0] = '\0';
            neuron_public_key[0]  = '\0';
            return;
        }
    } else {
        zlog_notice(neuron,
                    "Load external RSA key pair from config directory: %s",
                    dir_path);
    }
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
        if (!strcmp((char *) ptr->d_name, ".") ||
            !strcmp((char *) ptr->d_name, "..")) {
            continue;
        }

        if (strstr((char *) ptr->d_name, ".pem") != NULL ||
            strstr((char *) ptr->d_name, ".pub") != NULL) {
            char content[2047] = { 0 };
            assert(load_key_file(dir_path, (char *) ptr->d_name, content,
                                 sizeof(content)));
            add_key((char *) ptr->d_name, content);
        }
    }

    closedir(dir);
}

int neu_jwt_init(const char *dir_path)
{
    if (disable_jwt) {
        zlog_notice(neuron,
                    "HTTP auth is disabled, skip JWT RSA key generation");
        return 0;
    }

    load_neuron_key(dir_path);
    scanf_key("certs");
    return neuron_private_key[0] == '\0' || neuron_public_key[0] == '\0' ? -1
                                                                         : 0;
}

int neu_jwt_new(char **token, const char *user)
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

    // only normal user need to add user grant
    // do not change the original logic
    if (0 != strcmp(user, "admin")) {
        if (0 != jwt_add_grant(jwt, "user", user)) {
            goto err_out;
        }
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

void neu_jwt_decode_user_after_valid(char *bearer, char *user)
{
    char * token = &bearer[strlen("Bearar ")];
    jwt_t *jwt   = NULL;
    if (0 != jwt_decode(&jwt, token, NULL, 0)) {
        return;
    }

    const char *user_grant = jwt_get_grant(jwt, "user");
    if (NULL == user_grant) {
        const char *admin = "admin";
        strncpy(user, admin, strlen(admin));
        jwt_free(jwt);
        return;
    }

    strncpy(user, user_grant, strlen(user_grant));
    jwt_free(jwt);
}

void neu_jwt_destroy()
{
    // memset(token_local, 0, sizeof(token_local));
}
