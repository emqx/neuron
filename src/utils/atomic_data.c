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

#include <stdatomic.h>

#include "utils/atomic_data.h"

/*
 * atomic flag functions
 */
bool neu_atomic_flag_test_and_set(neu_atomic_flag *f)
{
    return (atomic_flag_test_and_set(&f->f));
}

void neu_atomic_flag_reset(neu_atomic_flag *f)
{
    atomic_flag_clear(&f->f);
}

/*
 * atomic bool functions
 */
void neu_atomic_set_bool(neu_atomic_bool *v, bool b)
{
    atomic_store(&v->v, b);
}

bool neu_atomic_get_bool(neu_atomic_bool *v)
{
    return (atomic_load(&v->v));
}

bool neu_atomic_swap_bool(neu_atomic_bool *v, bool b)
{
    return (atomic_exchange(&v->v, b));
}

void neu_atomic_init_bool(neu_atomic_bool *v)
{
    atomic_init(&v->v, false);
}

/*
 * atomic int functions
 */
void neu_atomic_init(neu_atomic_int *v)
{
    atomic_init(&v->v, 0);
}

void neu_atomic_add(neu_atomic_int *v, int bump)
{
    (void) atomic_fetch_add_explicit(&v->v, bump, memory_order_relaxed);
}

void neu_atomic_sub(neu_atomic_int *v, int bump)
{
    (void) atomic_fetch_sub_explicit(&v->v, bump, memory_order_relaxed);
}

int neu_atomic_get(neu_atomic_int *v)
{
    return (atomic_load(&v->v));
}

void neu_atomic_set(neu_atomic_int *v, int i)
{
    return (atomic_store(&v->v, i));
}

int neu_atomic_swap(neu_atomic_int *v, int i)
{
    return (atomic_exchange(&v->v, i));
}

void neu_atomic_inc(neu_atomic_int *v)
{
    atomic_fetch_add(&v->v, 1);
}

void neu_atomic_dec(neu_atomic_int *v)
{
    atomic_fetch_sub(&v->v, 1);
}

int neu_atomic_dec_nv(neu_atomic_int *v)
{
    return (atomic_fetch_sub(&v->v, 1) - 1);
}

bool neu_atomic_cas(neu_atomic_int *v, int comp, int new)
{
    return (atomic_compare_exchange_strong(&v->v, &comp, new));
}
