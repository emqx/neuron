/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2024 EMQ Technologies Co., Ltd All rights reserved.
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

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils/neu_path.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

bool neu_path_is_valid_component(const char *name)
{
    if (name == NULL || name[0] == '\0') {
        return false;
    }
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        return false;
    }
    // A safe component must not contain a path separator.
    if (strchr(name, '/') != NULL) {
        return false;
    }
    return true;
}

// Return true if `resolved` is `base` or lies below `base/`.
static bool is_within(const char *base, const char *resolved)
{
    size_t bl = strlen(base);
    if (strncmp(resolved, base, bl) != 0) {
        return false;
    }
    // Exact match, or the next char is a separator (a real sub-path).
    return resolved[bl] == '\0' || resolved[bl] == '/';
}

char *neu_path_confine(const char *base, const char *path)
{
    if (path == NULL || path[0] == '\0') {
        return NULL;
    }

    char base_real[PATH_MAX];
    if (base == NULL || base[0] == '\0') {
        base = ".";
    }
    if (realpath(base, base_real) == NULL) {
        return NULL;
    }

    // Build an absolute candidate: absolute paths are taken as-is (and must
    // still resolve inside base), relative paths are joined under base.
    char candidate[PATH_MAX];
    if (path[0] == '/') {
        if (strlen(path) >= sizeof(candidate)) {
            return NULL;
        }
        strcpy(candidate, path);
    } else {
        if ((size_t) snprintf(candidate, sizeof(candidate), "%s/%s", base_real,
                              path) >= sizeof(candidate)) {
            return NULL;
        }
    }

    char resolved[PATH_MAX];
    if (realpath(candidate, resolved) == NULL) {
        // The final component may not exist yet (e.g. a file about to be
        // created). Resolve the parent directory and re-append the basename.
        char tmp[PATH_MAX];
        strncpy(tmp, candidate, sizeof(tmp));
        tmp[sizeof(tmp) - 1] = '\0';

        char *slash = strrchr(tmp, '/');
        if (slash == NULL) {
            return NULL;
        }
        *slash            = '\0';
        const char *bname = slash + 1;
        if (!neu_path_is_valid_component(bname)) {
            return NULL;
        }

        char parent_real[PATH_MAX];
        if (realpath(tmp[0] != '\0' ? tmp : "/", parent_real) == NULL) {
            return NULL;
        }
        if (!is_within(base_real, parent_real)) {
            return NULL;
        }
        if ((size_t) snprintf(resolved, sizeof(resolved), "%s/%s", parent_real,
                              bname) >= sizeof(resolved)) {
            return NULL;
        }
        return strdup(resolved);
    }

    if (!is_within(base_real, resolved)) {
        return NULL;
    }
    return strdup(resolved);
}
