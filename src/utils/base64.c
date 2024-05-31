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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/bio.h>
#include <openssl/evp.h>

char *neu_encode64(const unsigned char *input, int length)
{
    const int      pl     = 4 * ((length + 2) / 3);
    unsigned char *output = calloc(pl + 1, 1);
    const int      ol     = EVP_EncodeBlock(output, input, length);

    assert(pl == ol);

    return (char *) output;
}

size_t calcDecodeLength(const char *b64input)
{ // Calculates the length of a decoded string
    size_t len = strlen(b64input), padding = 0;

    if (0 == len) {
        return 0;
    }

    if (b64input[len - 1] == '=' &&
        b64input[len - 2] == '=') // last two chars are =
        padding = 2;
    else if (b64input[len - 1] == '=') // last char is =
        padding = 1;

    return (len * 3) / 4 - padding;
}

int Base64Decode(const char *b64message, unsigned char **buffer, int *length)
{ // Decodes a base64 encoded string
    BIO *bio, *b64;

    int decodeLen        = calcDecodeLength(b64message);
    *buffer              = (unsigned char *) malloc(decodeLen + 1);
    (*buffer)[decodeLen] = '\0';

    bio = BIO_new_mem_buf(b64message, -1);
    b64 = BIO_new(BIO_f_base64());
    bio = BIO_push(b64, bio);

    BIO_set_flags(
        bio, BIO_FLAGS_BASE64_NO_NL); // Do not use newlines to flush buffer
    *length = BIO_read(bio, *buffer, strlen(b64message));
    BIO_free_all(bio);

    if (*length != decodeLen) {
        free(*buffer);
        *buffer = NULL;
        return -1;
    }

    return (0); // success
}

unsigned char *neu_decode64(int *length, const char *input)
{
    unsigned char *output;
    Base64Decode(input, &output, length);
    return output;
}