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

#ifndef NEURON_ATOMIC_H_
#define NEURON_ATOMIC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdatomic.h>
#include <stdbool.h>

// neu_atomic_flag supports only test-and-set and reset operations.
// This can be implemented without locks on any reasonable system, and
// it corresponds to C11 atomic flag.
struct neu_atomic_flag {
    atomic_flag f;
};
typedef struct neu_atomic_flag neu_atomic_flag;

extern bool neu_atomic_flag_test_and_set(neu_atomic_flag *);
extern void neu_atomic_flag_reset(neu_atomic_flag *);

// neu_atomic_bool is for boolean flags that need to be checked without
// changing their value.  This might require a lock on some systems.
struct neu_atomic_bool {
    atomic_bool v;
};
typedef struct neu_atomic_bool neu_atomic_bool;

extern void neu_atomic_init_bool(neu_atomic_bool *);
extern void neu_atomic_set_bool(neu_atomic_bool *, bool);
extern bool neu_atomic_get_bool(neu_atomic_bool *);
extern bool neu_atomic_swap_bool(neu_atomic_bool *, bool);

// In a lot of circumstances, we want a simple atomic reference count,
// or atomic tunable values for integers like queue lengths or TTLs.
// These native integer forms should be preferred over the 64 bit versions
// unless larger bit sizes are truly needed.  They will be more efficient
// on many platforms.
struct neu_atomic_int {
    atomic_int v;
};
typedef struct neu_atomic_int neu_atomic_int;

extern void neu_atomic_init(neu_atomic_int *);
extern void neu_atomic_add(neu_atomic_int *, int);
extern void neu_atomic_sub(neu_atomic_int *, int);
extern int  neu_atomic_get(neu_atomic_int *);
extern void neu_atomic_set(neu_atomic_int *, int);
extern int  neu_atomic_swap(neu_atomic_int *, int);
extern int  neu_atomic_dec_nv(neu_atomic_int *);
extern void neu_atomic_dec(neu_atomic_int *);
extern void neu_atomic_inc(neu_atomic_int *);

// neu_atomic_cas is a compare and swap.  The second argument is the
// value to compare against, and the third is the new value. Returns
// true if the value was set.
extern bool neu_atomic_cas(neu_atomic_int *, int, int);

#ifdef __cplusplus
}
#endif

#endif
