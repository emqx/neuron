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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define NEU_HAVE_BACKTRACE 1
#if NEU_HAVE_BACKTRACE
#include <execinfo.h>
#endif

#include "log.h"
#include "panic.h"

void neu_show_backtrace(void)
{
#if NEU_HAVE_BACKTRACE
    void *frames[50];
    int   nframes;

    nframes = backtrace(frames, sizeof(frames) / sizeof(frames[0]));
    if (nframes > 1) {
        char **lines = backtrace_symbols(frames, nframes);
        if (lines == NULL) {
            return;
        }
        for (int i = 1; i < nframes; i++) {
            log_fatal(lines[i]);
        }
        free(lines);
    }
#endif
}

/**
 * neu_panic shows a panic message, a possible stack bracktrace, then aborts
 * the process/program.  This should only be called when a condition arises
 * that should not be possible, e.g. a programming assertion failure. It should
 * not be called in situations such as ENOMEM, as neu_panic is fairly rude
 * to any application it may be called from within.
 */
void neu_panic(const char *fmt, ...)
{
    char    buf[100];
    char    fbuf[93]; // 7 bytes of "panic: "
    va_list va;

    va_start(va, fmt);
    (void) vsnprintf(fbuf, sizeof(fbuf), fmt, va);
    va_end(va);

    (void) snprintf(buf, sizeof(buf), "panic: %s", fbuf);

    log_fatal(buf);
    log_fatal("This message is indicative of a BUG.");
    log_fatal("Report this at https://github.com/neugates/neuron/issues");

    neu_show_backtrace();
    abort();
}
