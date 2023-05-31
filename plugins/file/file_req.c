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

#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "file_req.h"

struct file_group_data {
    UT_array *tags;
    char *    group;
};

static void plugin_group_free(neu_plugin_group_t *pgp);

int file_group_timer(neu_plugin_t *plugin, neu_plugin_group_t *group)
{
    neu_adapter_update_metric_cb_t update_metric =
        plugin->common.adapter_callbacks->update_metric;
    int64_t rtt = NEU_METRIC_LAST_RTT_MS_MAX;

    struct file_group_data *gd     = NULL;
    int                     length = plugin->file_length;

    if (group->user_data == NULL) {
        gd                = calloc(1, sizeof(struct file_group_data));
        group->user_data  = gd;
        group->group_free = plugin_group_free;
        utarray_new(gd->tags, &ut_ptr_icd);

        gd->group = strdup(group->group_name);
    }

    utarray_foreach(group->tags, neu_datatag_t *, tag)
    {
        FILE *       f      = NULL;
        struct stat  st     = { 0 };
        char *       buf    = calloc(1, length);
        neu_dvalue_t dvalue = { 0 };

        if (stat(tag->address, &st) != 0) {
            dvalue.type      = NEU_TYPE_ERROR;
            dvalue.value.i32 = NEU_ERR_FILE_NOT_EXIST;
            goto dvalue_result;
        }

        if ((f = fopen(tag->address, "rb+")) == NULL) {
            nlog_warn("fail open to read and write file: %s", tag->address);
            dvalue.type      = NEU_TYPE_ERROR;
            dvalue.value.i32 = NEU_ERR_FILE_OPEN_FAILURE;
            goto dvalue_result;
        }

        fseek(f, 0, SEEK_END);
        int size = ftell(f);
        if (size >= length) {
            nlog_warn(
                "the file length exceeds the set value, file: %s, length: %d",
                tag->address, size);
            dvalue.type      = NEU_TYPE_ERROR;
            dvalue.value.i32 = NEU_ERR_STRING_TOO_LONG;

            fclose(f);
            goto dvalue_result;
        }

        fseek(f, 0, SEEK_SET);
        int ret = fread(buf, sizeof(char), length, f);
        if (ret == 0) {
            nlog_warn("read file failed:%s", tag->address);
            dvalue.type      = NEU_TYPE_ERROR;
            dvalue.value.i32 = NEU_ERR_FILE_READ_FAILURE;

            fclose(f);
            goto dvalue_result;
        } else if (ret >= NEU_VALUE_SIZE) {
            nlog_warn("the file length exceeds the maximum, file: %s, ret: %d",
                      tag->address, ret);
            dvalue.type      = NEU_TYPE_ERROR;
            dvalue.value.i32 = NEU_ERR_STRING_TOO_LONG;

            fclose(f);
            goto dvalue_result;
        }

        dvalue.type = NEU_TYPE_STRING;
        strncpy(dvalue.value.str, buf, strlen(buf));
        rtt = 1;
        fclose(f);

    dvalue_result:
        plugin->common.adapter_callbacks->driver.update(
            plugin->common.adapter, group->group_name, tag->name, dvalue);

        free(buf);
    }

    update_metric(plugin->common.adapter, NEU_METRIC_LAST_RTT_MS, rtt, NULL);

    return 0;
}

static void plugin_group_free(neu_plugin_group_t *pgp)
{
    struct file_group_data *gd = (struct file_group_data *) pgp->user_data;

    utarray_foreach(gd->tags, neu_datatag_t **, tag) { free(*tag); }

    utarray_free(gd->tags);
    free(gd->group);

    free(gd);
}

int file_write(neu_plugin_t *plugin, void *req, neu_datatag_t *tag,
               neu_value_u value)
{
    FILE *fp     = NULL;
    int   ret    = 0;
    int   length = strlen(value.str);

    if (length >= NEU_VALUE_SIZE) {
        nlog_warn("write file exceeds the maximum value, file: %s, length: %d",
                  tag->address, length);
        ret = NEU_ERR_STRING_TOO_LONG;

        goto dvalue_result;
    }

    fp = fopen(tag->address, "w");

    if (0 == fwrite(value.str, sizeof(char), strlen(value.str), fp)) {
        nlog_warn("write file failed: %s", tag->address);
        ret = NEU_ERR_FILE_WRITE_FAILURE;

        fclose(fp);
        goto dvalue_result;
    }

    fclose(fp);
    neu_datatag_string_ltoh(value.str, strlen(value.str));

dvalue_result:
    plugin->common.adapter_callbacks->driver.write_response(
        plugin->common.adapter, req, ret);

    return 0;
}