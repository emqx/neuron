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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/evp.h>

char *neu_encode64(const unsigned char *input, int length)
{
    const int      pl     = 4 * ((length + 2) / 3);
    unsigned char *output = calloc(pl + 1, 1);
    const int      ol     = EVP_EncodeBlock(output, input, length);

    assert(pl == ol);

    return (char *) output;
}

unsigned char *neu_decode64(int *length, const char *input)
{
    size_t         len    = strlen(input);
    const int      pl     = 3 * len / 4;
    unsigned char *output = calloc(pl, 1);
    const int ol = EVP_DecodeBlock(output, (const unsigned char *) input, len);

    assert(pl == ol);

    *length = ol;
    return output;
}