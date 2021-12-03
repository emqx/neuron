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
config_ **/

#include "neu_tag.h"

bool neu_tag_check_attribute(neu_attribute_e attribute)
{
    return !((attribute & NEU_ATTRIBUTE_READ) == 0 &&
             (attribute & NEU_ATTRIBUTE_WRITE) == 0 &&
             (attribute & NEU_ATTRIBUTE_SUBSCRIBE) == 0);
}

neu_datatag_t *neu_datatag_alloc(neu_attribute_e attr, neu_dtype_e type,
                                 neu_addr_str_t addr, const char *name)
{
    neu_datatag_t *tag = malloc(sizeof(neu_datatag_t));

    if (NULL == tag) {
        return NULL;
    }

    tag->id        = 0;
    tag->attribute = attr;
    tag->type      = type;

    tag->addr_str = strdup(addr);
    if (NULL == tag->addr_str) {
        goto error;
    }

    tag->name = strdup(name);
    if (NULL == tag->name) {
        goto error;
    }

    return tag;

error:
    free(tag->addr_str);
    free(tag->name);
    free(tag);
    return NULL;
}

void neu_datatag_free(neu_datatag_t *datatag)
{
    if (datatag) {
        free(datatag->addr_str);
        free(datatag->name);
    }
    free(datatag);
}
