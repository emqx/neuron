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

#include <dirent.h>
#include <stdio.h>

#include "define.h"
#include "utils/log.h"
#include "utils/utarray.h"
#include "utils/utextend.h"

/**
 * 收集指定节点的所有日志文件
 *
 * @param dir 日志文件所在目录
 * @param node 节点名称
 * @return 包含所有匹配日志文件名的数组，失败返回NULL
 */
static UT_array *collect_logs(const char *dir, const char *node)
{
    DIR *          dirp                        = NULL;
    struct dirent *dent                        = NULL;
    UT_array *     files                       = NULL;
    char           flag[NEU_NODE_NAME_LEN + 2] = { 0 };

    if ((dirp = opendir(dir)) == NULL) {
        nlog_error("fail open dir: %s", dir);
        return NULL;
    }

    utarray_new(files, &ut_str_icd);
    snprintf(flag, sizeof(flag), "%s.", node);

    while (NULL != (dent = readdir(dirp))) {
        if (strstr(dent->d_name, flag) != NULL) {
            char *file = dent->d_name;
            utarray_push_back(files, &file);
        }
    }

    closedir(dirp);
    return files;
}

/**
 * 删除指定节点的所有日志文件
 *
 * @param node 节点名称
 */
void remove_logs(const char *node)
{
    UT_array *files = collect_logs("./logs", node);
    if (files != NULL) {
        if (0 == utarray_len(files)) {
            nlog_warn("directory logs contains no log files of %s", node);
        }

        utarray_foreach(files, char **, file)
        {
            char path[NEU_NODE_NAME_LEN + 10] = { 0 };
            snprintf(path, sizeof(path), "./logs/%s", *file);
            if (remove(path) != 0) {
                nlog_warn("rm %s file fail", path);
            }
        }
        utarray_free(files);
    }
}
