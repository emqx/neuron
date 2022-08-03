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

#ifndef __NEU_BASE64_H__
#define __NEU_BASE64_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief base64 encoding.
 *
 * @param[in] input Data to be encoded.
 * @param[in] length Length of data to be encoded.
 * @return Encoded data.
 */

char *neu_encode64(const unsigned char *input, int length);

/**
 * @brief base64 decoding.
 *
 * @param[out] length Pointer to the length of the decoded data.
 * @param[in] input Data to be decoded.
 * @return The decoded data.
 */
unsigned char *neu_decode64(int *length, const char *input);

#ifdef __cplusplus
}
#endif

#endif // __NEU_BASE64_H__
