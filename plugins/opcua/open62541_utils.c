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

#include <neuron.h>
#include <stdio.h>
#include <stdlib.h>

#include "open62541_utils.h"

#define OPEN62541_LOG_LEN 256

const char *log_category[7] = { "network",       "channel", "session",
                                "server",        "client",  "userland",
                                "securitypolicy" };

static void open62541_log(void *context, UA_LogLevel level,
                          UA_LogCategory category, const char *msg,
                          va_list args)
{
    UNUSED(args);

    if (NULL != context && (UA_LogLevel)(uintptr_t) context > level) {
        return;
    }

    char str[OPEN62541_LOG_LEN] = { '\0' };
    vsnprintf(str, OPEN62541_LOG_LEN - 1, msg, args);

    switch (level) {
    case UA_LOGLEVEL_TRACE:
        log_trace("[%s]%s", log_category[category], str);
        break;
    case UA_LOGLEVEL_DEBUG:
        log_debug("[%s]%s", log_category[category], str);
        break;
    case UA_LOGLEVEL_INFO:
        log_info("[%s]%s", log_category[category], str);
        break;
    case UA_LOGLEVEL_WARNING:
        log_warn("[%s]%s", log_category[category], str);
        break;
    case UA_LOGLEVEL_ERROR:
        log_error("[%s]%s", log_category[category], str);
        break;
    case UA_LOGLEVEL_FATAL:
        log_fatal("[%s]%s", log_category[category], str);
        break;
    };
}

static void open62541_log_clear(void *context)
{
    UNUSED(context);
}

UA_Logger open62541_log_with_level(UA_LogLevel min_level)
{
    UA_Logger logger = { open62541_log, (void *) min_level,
                         open62541_log_clear };
    return logger;
}

UA_ByteString client_load_file(const char *const path)
{
    UA_ByteString file_contents = UA_STRING_NULL;

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        errno = 0;
        return file_contents;
    }

    fseek(fp, 0, SEEK_END);
    file_contents.length = (size_t) ftell(fp);
    file_contents.data =
        (UA_Byte *) UA_malloc(file_contents.length * sizeof(UA_Byte));
    if (file_contents.data) {
        fseek(fp, 0, SEEK_SET);
        size_t read = fread(file_contents.data, sizeof(UA_Byte),
                            file_contents.length, fp);
        if (read != file_contents.length)
            UA_ByteString_clear(&file_contents);
    } else {
        file_contents.length = 0;
    }

    fclose(fp);
    return file_contents;
}