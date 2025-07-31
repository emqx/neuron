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

/**
 * @brief 将二进制数据编码为Base64字符串
 *
 * @param input 需要编码的二进制数据
 * @param length 二进制数据的长度
 * @return char* 返回Base64编码后的字符串，使用后需要调用者释放
 */
char *neu_encode64(const unsigned char *input, int length)
{
    const int      pl     = 4 * ((length + 2) / 3);
    unsigned char *output = calloc(pl + 1, 1);
    const int      ol     = EVP_EncodeBlock(output, input, length);

    assert(pl == ol);

    return (char *) output;
}

/**
 * @brief 计算Base64解码后的数据长度
 *
 * @param b64input Base64编码的字符串
 * @return size_t 解码后的数据长度
 */
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

/**
 * @brief 解码Base64字符串到二进制数据
 *
 * @param b64message Base64编码的字符串
 * @param buffer 用于存储解码后数据的缓冲区指针的地址
 * @param length 解码后数据长度的指针
 * @return int 成功返回0
 */
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
    assert(*length == decodeLen); // length should equal decodeLen, else
                                  // something went horribly wrong
    BIO_free_all(bio);

    return (0); // success
}

/**
 * @brief 将Base64字符串解码为二进制数据
 *
 * @param length 用于存储解码后数据长度的指针
 * @param input Base64编码的字符串
 * @return unsigned char* 解码后的二进制数据，使用后需要调用者释放
 */
unsigned char *neu_decode64(int *length, const char *input)
{
    unsigned char *output;
    Base64Decode(input, &output, length);
    return output;
}