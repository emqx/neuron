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

#include "file.h"
#include "log.h"
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#define DEV_TTY_LENTH 261

int tty_file_list_get(char ***tty_file)
{
    DIR *          dir    = NULL;
    struct dirent *ptr    = NULL;
    int            n_file = 0;

    *tty_file = NULL;
    dir       = opendir("/dev");
    if (dir == NULL) {
        log_error("Open dir error: %s", strerror(errno));
        return -1;
    }
    ptr = readdir(dir);
    while (ptr != NULL) {
        if (ptr->d_type == 2) {
            char dev_tty[DEV_TTY_LENTH] = { 0 };
            if (strstr(ptr->d_name, "tty") != NULL) {
                n_file += 1;
                *tty_file = realloc(*tty_file, sizeof(char *) * n_file);
                snprintf(dev_tty, sizeof(dev_tty), "/dev/%s", ptr->d_name);
                (*tty_file)[n_file - 1] = strdup(dev_tty);
            }
        }
        ptr = readdir(dir);
    }

    closedir(dir);

    return n_file;
}

char *file_string_read(size_t *length, const char *const path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        errno   = 0;
        *length = 0;
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    *length = (size_t) ftell(fp);
    if (0 == *length) {
        fclose(fp);
        return NULL;
    }

    char *data = NULL;
    data       = (char *) malloc((*length + 1) * sizeof(char));
    if (NULL != data) {
        data[*length] = '\0';
        fseek(fp, 0, SEEK_SET);
        size_t read = fread(data, sizeof(char), *length, fp);
        if (read != *length) {
            *length = 0;
            free(data);
            data = NULL;
        }
    } else {
        *length = 0;
    }

    fclose(fp);
    return data;
}