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
#include <stdlib.h>

#include "define.h"

#include "sub_msg.h"

struct sub_app {
    neu_subscribe_type_e type;
    UT_array *           apps;
};

struct neu_sub_msg_mgr {
    struct sub_app *sub_apps;
};

static UT_icd app_icd = { NEU_NODE_NAME_LEN, NULL, NULL, NULL };

neu_sub_msg_mgr_t *neu_sub_msg_manager_create()
{
    neu_sub_msg_mgr_t *mgr = calloc(1, sizeof(neu_sub_msg_mgr_t));

    mgr->sub_apps = calloc(NEU_APP_SUBSCRIBE_MSG_SIZE, sizeof(struct sub_app));
    for (int i = 0; i < NEU_APP_SUBSCRIBE_MSG_SIZE; i++) {
        utarray_new(mgr->sub_apps[i].apps, &app_icd);
    }

    mgr->sub_apps[0].type = NEU_SUBSCRIBE_NODES_STATE;

    return mgr;
}

void neu_sub_msg_manager_destroy(neu_sub_msg_mgr_t *mgr)
{
    for (int i = 0; i < NEU_APP_SUBSCRIBE_MSG_SIZE; i++) {
        utarray_free(mgr->sub_apps[i].apps);
    }

    free(mgr->sub_apps);
    free(mgr);
}

// app name  array
UT_array *neu_sub_msg_manager_get(neu_sub_msg_mgr_t *  mgr,
                                  neu_subscribe_type_e type)
{
    UT_array *result = NULL;

    for (int i = 0; i < NEU_APP_SUBSCRIBE_MSG_SIZE; i++) {
        if (mgr->sub_apps[i].type == type) {
            if (utarray_len(mgr->sub_apps[i].apps) > 0) {
                result = utarray_clone(mgr->sub_apps[i].apps);
            }
            break;
        }
    }

    return result;
}

void neu_sub_msg_manager_add(neu_sub_msg_mgr_t *mgr, neu_subscribe_type_e type,
                             const char *app)
{
    char app_name[NEU_NODE_NAME_LEN] = { 0 };

    for (int i = 0; i < NEU_APP_SUBSCRIBE_MSG_SIZE; i++) {
        if (mgr->sub_apps[i].type == type) {
            strcpy(app_name, app);
            utarray_push_back(mgr->sub_apps[i].apps, app_name);
        }
    }
}

void neu_sub_msg_manager_del(neu_sub_msg_mgr_t *mgr, neu_subscribe_type_e type,
                             const char *app)
{
    for (int i = 0; i < NEU_APP_SUBSCRIBE_MSG_SIZE; i++) {
        if (mgr->sub_apps[i].type == type) {
            utarray_foreach(mgr->sub_apps[i].apps, char *, app_name)
            {
                if (strcmp(app_name, app) == 0) {
                    utarray_erase(
                        mgr->sub_apps[i].apps,
                        utarray_eltidx(mgr->sub_apps[i].apps, app_name), 1);
                    break;
                }
            }
            break;
        }
    }
}
