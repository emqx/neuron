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

#if defined(__GNUC__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#if (defined(__unix__) || defined(unix)) && !defined(USG)
#include <sys/param.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#include "utils/asprintf.h"

int neu_asprintf(char **strp, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int rv = neu_vasprintf(strp, fmt, ap);
    va_end(ap);
    return rv;
}

int neu_vasprintf(char **strp, const char *fmt, va_list ap)
{
#if defined(_GNU_SOURCE) || defined(BSD) || defined(__APPLE__)
    return vasprintf(strp, fmt, ap);
#else
    int   n = 1 + vsnprintf(NULL, 0, fmt, ap);
    char *s = malloc(n);
    if (NULL == s) {
        return -1;
    }

    n     = vsnprintf(s, n, fmt, ap);
    *strp = s;
    return n;
#endif
}
