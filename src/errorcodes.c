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

#include <types.h>
#include <errorcodes.h>

typedef struct {
    neu_errorcode_t code;
    const char *name;
} neu_errorcodename_t;

static const size_t errorcodeDescriptionsSize = 2;
static const neu_errorcodename_t errorcodeDescriptions[2] = {
    {NEU_ERRORCODE_NONE, "None"},
    {NEU_ERRORCODE_OUTOFMEMORY, "ErrorCodeOutOfMemory"}
};

const char * neu_errorcode_getName(neu_errorcode_t code) {
    for (size_t i = 0; i < errorcodeDescriptionsSize; ++i) {
        if (errorcodeDescriptions[i].code == code)
            return errorcodeDescriptions[i].name;
    }
    return errorcodeDescriptions[errorcodeDescriptionsSize-1].name;
}

