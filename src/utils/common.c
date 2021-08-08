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

#include "common.h"
#include "unistd.h"
#include <string.h>

#if defined(NEU_PLATFORM_DARWIN)

#include <mach-o/dyld.h>

char *get_self_path(char *dest)
{
    char self[MAX_PATH_SZ] = { 0 };

    uint32_t count = sizeof self;

    if (_NSGetExecutablePath(self, &count) != 0) {
        return NULL;
    }

    count    = strlen(self);
    char *br = strrchr(self, '/');

    self[count - strlen(br)] = '\0';
    strcpy(dest, self);
    return dest;
}

#elif defined(NEU_PLATFORM_WINDOWS)

#include <windows.h>

char *get_self_path(char *dest)
{
    char self[MAX_PATH] = { 0 };

    SetLastError(0);
    int count = GetModuleFileNameA(NULL, self, MAX_PATH);

    if (count == 0 || count == MAX_PATH) {
        return NULL;
    }

    char *br = strrchr(self, '\\');

    self[count - strlen(br)] = '\0';
    strcpy(dest, self);

    return dest;
}

#elif defined(NEU_PLATFORM_LINUX)

char *get_self_path(char *dest)
{
    char self[MAX_PATH_SZ] = { 0 };

    int count = readlink("/proc/self/exe", self, sizeof self);

    if (count < 0 || count >= MAX_PATH_SZ) {
        return NULL;
    }

    char *br = strrchr(self, '/');

    self[count - strlen(br)] = '\0';
    strcpy(dest, self);

    return dest;
}

#endif
