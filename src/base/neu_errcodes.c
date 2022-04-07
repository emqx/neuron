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

#include "errcodes.h"
#include "stdio.h"
#include "stdlib.h"

static const struct {
    int         code;
    const char *msg;
} neu_errors[] = {
    // clang-format off
    { 0, "Successful" },
    { NEU_ERR_EINTR, "Interrupted" },
    { NEU_ERR_ENOMEM, "Out of memory" },
    { NEU_ERR_EINVAL, "Invalid argument" },
    { NEU_ERR_EBUSY, "Resource busy" },
    { NEU_ERR_ETIMEDOUT, "Timed out" },
    { NEU_ERR_ECONNREFUSED, "Connection refused" },
    { NEU_ERR_ECLOSED, "Object closed" },
    { NEU_ERR_EAGAIN, "Try again" },
    { NEU_ERR_ENOTSUP, "Not supported" },
    { NEU_ERR_EADDRINUSE, "Address in use" },
    { NEU_ERR_ESTATE, "Incorrect state" },
    { NEU_ERR_ENOENT, "Entry not found" },
    { NEU_ERR_EPROTO, "Protocol error" },
    { NEU_ERR_EUNREACHABLE, "Destination unreachable" },
    { NEU_ERR_EADDRINVAL, "Address invalid" },
    { NEU_ERR_EPERM, "Permission denied" },
    { NEU_ERR_EMSGSIZE, "Message too large" },
    { NEU_ERR_ECONNRESET, "Connection reset" },
    { NEU_ERR_ECONNABORTED, "Connection aborted" },
    { NEU_ERR_ECANCELED, "Operation canceled" },
    { NEU_ERR_ENOFILES, "Out of files" },
    { NEU_ERR_ENOSPC, "Out of space" },
    { NEU_ERR_EEXIST, "Resource already exists" },
    { NEU_ERR_EREADONLY, "Read only resource" },
    { NEU_ERR_EWRITEONLY, "Write only resource" },
    { NEU_ERR_ECRYPTO, "Cryptographic error" },
    { NEU_ERR_EPEERAUTH, "Peer could not be authenticated" },
    { NEU_ERR_ENOARG, "Option requires argument" },
    { NEU_ERR_EAMBIGUOUS, "Ambiguous option" },
    { NEU_ERR_EBADTYPE, "Incorrect type" },
    { NEU_ERR_ECONNSHUT, "Connection shutdown" },
    { NEU_ERR_EINTERNAL, "Internal error detected" },
    { 0, NULL },
    // clang-format on
};

// Misc.
const char *neu_strerror(int num)
{
    static char unknownerrbuf[32];
    for (int i = 0; neu_errors[i].msg != NULL; i++) {
        if (neu_errors[i].code == num) {
            return (neu_errors[i].msg);
        }
    }

    (void) snprintf(unknownerrbuf, sizeof(unknownerrbuf), "Unknown error #%d",
                    num);
    return (unknownerrbuf);
}
