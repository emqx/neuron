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

#ifndef NEURON_UTILS_PATH_H
#define NEURON_UTILS_PATH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

// Reject an untrusted path component (e.g. a node/plugin/library name that is
// used to build a filesystem path). Returns true only when `name` is a single,
// safe path segment: non-empty, contains no '/' and is not "." or "..".
bool neu_path_is_valid_component(const char *name);

// Confine an untrusted `path` to the trusted base directory `base`.
//
// The result is the canonical absolute path when it resolves inside `base`
// (either a relative path joined under `base`, or an absolute path that already
// lies within `base`). Returns a newly heap-allocated string the caller must
// free(), or NULL if the path escapes `base`, is invalid, or cannot be
// resolved. A not-yet-existing final component (e.g. a file about to be
// created) is handled by resolving its parent directory.
//
// If `base` is NULL or empty, the current working directory is used.
char *neu_path_confine(const char *base, const char *path);

#ifdef __cplusplus
}
#endif

#endif
