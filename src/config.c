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
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <yaml.h>

#include "log.h"

#include "config.h"

static char *config = NULL;

int neu_config_init(const char *config_path)
{
    FILE *file = fopen(config_path, "r");

    if (file == NULL) {
        log_error("open config %s fail, error: %d", config_path, errno);
        return -1;
    }

    fclose(file);
    config = strdup(config_path);
    return 0;
}

void neu_config_uninit()
{
    free(config);
}

static int find_first_key(yaml_parser_t *parser, char *key)
{
    yaml_token_t token = { 0 };

    do {
        yaml_parser_scan(parser, &token);

        if (token.type == YAML_KEY_TOKEN) {
            yaml_token_delete(&token);
            yaml_parser_scan(parser, &token);

            if (token.type == YAML_SCALAR_TOKEN &&
                strncmp((char *) token.data.scalar.value, key, strlen(key)) ==
                    0) {
                yaml_token_delete(&token);
                return 0;
            }
        }

        if (token.type == YAML_STREAM_END_TOKEN) {
            yaml_token_delete(&token);
            return -1;
        }

        yaml_token_delete(&token);
    } while (1);

    return -1;
}

static int find_next_key(yaml_parser_t *parser, char *key)
{
    return find_first_key(parser, key);
}

char *neu_config_get_value(int n_key, char *key, ...)
{
    yaml_token_t  token   = { 0 };
    yaml_token_t  token_v = { 0 };
    yaml_parser_t parser  = { 0 };
    char *        result  = NULL;
    FILE *        file    = fopen(config, "r");

    if (file == NULL) {
        log_error("open config %s fail, error: %d", config, errno);
        return NULL;
    }

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_file(&parser, file);

    va_list kv;
    va_start(kv, key);

    if (find_first_key(&parser, key) != 0) {
        yaml_parser_delete(&parser);
        fclose(file);
        return NULL;
    }

    for (int i = 1; i < n_key; i++) {
        char *tk = va_arg(kv, char *);
        if (find_next_key(&parser, tk) != 0) {
            yaml_parser_delete(&parser);
            fclose(file);
            return NULL;
        }
    }

    va_end(kv);

    yaml_parser_scan(&parser, &token);
    yaml_parser_scan(&parser, &token_v);
    if (token.type == YAML_VALUE_TOKEN && token_v.type == YAML_SCALAR_TOKEN) {
        result = strdup((char *) token_v.data.scalar.value);
    }

    yaml_token_delete(&token);
    yaml_token_delete(&token_v);
    yaml_parser_delete(&parser);

    fclose(file);
    return result;
}