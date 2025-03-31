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

#include "json/json.h"
#include "json/neu_json_param.h"

#include "datalayers_config.h"
#include "datalayers_plugin.h"

int datalayers_config_parse(neu_plugin_t *plugin, const char *setting,
                            datalayers_config_t *config)
{
    int         ret         = 0;
    char *      err_param   = NULL;
    const char *placeholder = "********";

    neu_json_elem_t host     = { .name = "host", .t = NEU_JSON_STR };
    neu_json_elem_t port     = { .name = "port", .t = NEU_JSON_INT };
    neu_json_elem_t username = { .name = "username", .t = NEU_JSON_STR };
    neu_json_elem_t password = { .name = "password", .t = NEU_JSON_STR };

    if (NULL == setting || NULL == config) {
        plog_error(plugin, "invalid argument, null pointer");
        return -1;
    }

    ret = neu_parse_param(setting, &err_param, 2, &host, &port);
    if (0 != ret) {
        plog_error(plugin, "parsing setting fail, key: `%s`", err_param);
        goto error;
    }

    // host, required
    if (0 == strlen(host.v.val_str)) {
        plog_error(plugin, "setting invalid host: `%s`", host.v.val_str);
        goto error;
    }

    // port, required
    if (0 == port.v.val_int || port.v.val_int > 65535) {
        plog_error(plugin, "setting invalid port: %" PRIi64, port.v.val_int);
        goto error;
    }

    ret = neu_parse_param(setting, NULL, 1, &username);
    if (0 != ret) {
        plog_notice(plugin, "setting no username");
        goto error;
    }

    ret = neu_parse_param(setting, NULL, 1, &password);
    if (0 != ret) {
        plog_notice(plugin, "setting no password");
        goto error;
    }

    config->host     = host.v.val_str;
    config->port     = port.v.val_int;
    config->username = username.v.val_str;
    config->password = password.v.val_str;

    plog_notice(plugin, "config host            : %s", config->host);
    plog_notice(plugin, "config port            : %" PRIu16, config->port);

    if (config->username) {
        plog_notice(plugin, "config username        : %s", config->username);
    }
    if (config->password) {
        plog_notice(plugin, "config password        : %s",
                    0 == strlen(config->password) ? "" : placeholder);
    }

    return 0;

error:
    free(err_param);
    free(host.v.val_str);
    free(username.v.val_str);
    free(password.v.val_str);
    return -1;
}

void datalayers_config_fini(datalayers_config_t *config)
{
    free(config->host);
    free(config->username);
    free(config->password);

    memset(config, 0, sizeof(*config));
}