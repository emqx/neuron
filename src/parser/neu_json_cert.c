/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2024 EMQ Technologies Co., Ltd All rights reserved.
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

#include <jansson.h>

#include "errcodes.h"
#include "neu_json_cert.h"
#include "json/json.h"

int neu_json_decode_server_cert_self_sign_req(
    char *buf, neu_json_server_cert_self_sign_req_t **result)
{
    int                                   ret = 0;
    neu_json_server_cert_self_sign_req_t *req =
        calloc(1, sizeof(neu_json_server_cert_self_sign_req_t));
    if (req == NULL) {
        return NEU_ERR_EINTERNAL;
    }

    neu_json_elem_t req_elems[] = {
        {
            .name = "node",
            .t    = NEU_JSON_STR,
        },
    };

    ret = neu_json_decode(buf, NEU_JSON_ELEM_SIZE(req_elems), req_elems);
    if (ret != 0) {
        goto decode_fail;
    }

    req->app_name   = req_elems[0].v.val_str;
    req->days_valid = 36500; // default to 100 years

    *result = req;
    return 0;

decode_fail:
    neu_json_decode_server_cert_self_sign_req_free(req);
    return ret;
}

void neu_json_decode_server_cert_self_sign_req_free(
    neu_json_server_cert_self_sign_req_t *req)
{
    if (req != NULL) {
        free(req->app_name);
        free(req);
    }
}

int neu_json_decode_server_cert_upload_req(
    char *buf, neu_json_server_cert_upload_req_t **result)
{
    int                                ret = 0;
    neu_json_server_cert_upload_req_t *req =
        calloc(1, sizeof(neu_json_server_cert_upload_req_t));
    if (req == NULL) {
        return NEU_ERR_EINTERNAL;
    }

    neu_json_elem_t req_elems[] = {
        {
            .name = "node",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "certificate",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "privateKey",
            .t    = NEU_JSON_STR,
        },
    };

    ret = neu_json_decode(buf, NEU_JSON_ELEM_SIZE(req_elems), req_elems);
    if (ret != 0) {
        goto decode_fail;
    }

    req->app_name    = req_elems[0].v.val_str;
    req->certificate = req_elems[1].v.val_str;
    req->private_key = req_elems[2].v.val_str;

    *result = req;
    return 0;

decode_fail:
    neu_json_decode_server_cert_upload_req_free(req);
    return ret;
}

void neu_json_decode_server_cert_upload_req_free(
    neu_json_server_cert_upload_req_t *req)
{
    if (req != NULL) {
        free(req->app_name);
        free(req->certificate);
        free(req->private_key);
        free(req);
    }
}

int neu_json_decode_client_cert_delete_req(
    char *buf, neu_json_client_cert_delete_req_t **result)
{
    int                                ret = 0;
    neu_json_client_cert_delete_req_t *req =
        calloc(1, sizeof(neu_json_client_cert_delete_req_t));
    if (req == NULL) {
        return NEU_ERR_EINTERNAL;
    }

    neu_json_elem_t req_elems[] = {
        {
            .name = "node",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "fingerprint",
            .t    = NEU_JSON_STR,
        },
    };

    ret = neu_json_decode(buf, NEU_JSON_ELEM_SIZE(req_elems), req_elems);
    if (ret != 0) {
        goto decode_fail;
    }

    req->app_name    = req_elems[0].v.val_str;
    req->fingerprint = req_elems[1].v.val_str;

    *result = req;
    return 0;

decode_fail:
    neu_json_decode_client_cert_delete_req_free(req);
    return ret;
}

void neu_json_decode_client_cert_delete_req_free(
    neu_json_client_cert_delete_req_t *req)
{
    if (req != NULL) {
        free(req->app_name);
        free(req->fingerprint);
        free(req);
    }
}

int neu_json_decode_client_cert_trust_req(
    char *buf, neu_json_client_cert_trust_req_t **result)
{
    int                               ret = 0;
    neu_json_client_cert_trust_req_t *req =
        calloc(1, sizeof(neu_json_client_cert_trust_req_t));
    if (req == NULL) {
        return NEU_ERR_EINTERNAL;
    }

    neu_json_elem_t req_elems[] = {
        {
            .name = "node",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "fingerprint",
            .t    = NEU_JSON_STR,
        },
    };

    ret = neu_json_decode(buf, NEU_JSON_ELEM_SIZE(req_elems), req_elems);
    if (ret != 0) {
        goto decode_fail;
    }

    req->app_name    = req_elems[0].v.val_str;
    req->fingerprint = req_elems[1].v.val_str;

    *result = req;
    return 0;

decode_fail:
    neu_json_decode_client_cert_trust_req_free(req);
    return ret;
}

void neu_json_decode_client_cert_trust_req_free(
    neu_json_client_cert_trust_req_t *req)
{
    if (req != NULL) {
        free(req->app_name);
        free(req->fingerprint);
        free(req);
    }
}

int neu_json_decode_client_cert_upload_req(
    char *buf, neu_json_client_cert_upload_req_t **result)
{
    int                                ret = 0;
    neu_json_client_cert_upload_req_t *req =
        calloc(1, sizeof(neu_json_client_cert_upload_req_t));
    if (req == NULL) {
        return NEU_ERR_EINTERNAL;
    }

    neu_json_elem_t req_elems[] = {
        {
            .name = "node",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "certificate",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "isCa",
            .t    = NEU_JSON_INT,
        },
    };

    ret = neu_json_decode(buf, NEU_JSON_ELEM_SIZE(req_elems), req_elems);
    if (ret != 0) {
        neu_json_decode_client_cert_upload_req_free(req);
        return ret;
    }

    req->app_name    = req_elems[0].v.val_str;
    req->certificate = req_elems[1].v.val_str;
    req->is_ca       = req_elems[2].v.val_int;

    *result = req;
    return 0;
}

void neu_json_decode_client_cert_upload_req_free(
    neu_json_client_cert_upload_req_t *req)
{
    if (req != NULL) {
        free(req->app_name);
        free(req->certificate);
        free(req);
    }
}

int neu_json_encode_server_cert_info_resp(void *json_obj, void *param)
{
    neu_json_server_cert_info_resp_t *resp =
        (neu_json_server_cert_info_resp_t *) param;

    neu_json_elem_t resp_elems[] = {
        {
            .name      = "subject",
            .t         = NEU_JSON_STR,
            .v.val_str = resp->subject,
        },
        {
            .name      = "fingerprint",
            .t         = NEU_JSON_STR,
            .v.val_str = resp->fingerprint,
        },
        {
            .name      = "issuer",
            .t         = NEU_JSON_STR,
            .v.val_str = resp->issuer,
        },
        {
            .name      = "validTo",
            .t         = NEU_JSON_STR,
            .v.val_str = resp->valid_to,
        },
    };

    if (0 !=
        neu_json_encode_field(json_obj, resp_elems,
                              NEU_JSON_ELEM_SIZE(resp_elems))) {
        return -1;
    }

    return 0;
}

int neu_json_encode_client_cert_info_resp(void **json_obj, void *param)
{
    neu_json_client_cert_info_resp_t *resp =
        (neu_json_client_cert_info_resp_t *) param;
    void *result_array = neu_json_array();
    if (NULL == result_array) {
        return -1;
    }

    for (int i = 0; i < resp->count; i++) {
        neu_json_client_cert_info_t *cert        = resp->certs[i];
        void *                       cert_object = neu_json_encode_new();
        if (NULL == cert_object) {
            neu_json_encode_free(result_array);
            return -1;
        }

        neu_json_elem_t cert_elems[] = {
            {
                .name      = "subject",
                .t         = NEU_JSON_STR,
                .v.val_str = cert->subject,
            },
            {
                .name      = "fingerprint",
                .t         = NEU_JSON_STR,
                .v.val_str = cert->fingerprint,
            },
            {
                .name      = "issuer",
                .t         = NEU_JSON_STR,
                .v.val_str = cert->issuer,
            },
            {
                .name      = "validTo",
                .t         = NEU_JSON_STR,
                .v.val_str = cert->valid_to,
            },
            {
                .name      = "trustStatus",
                .t         = NEU_JSON_INT,
                .v.val_int = cert->trust_status,
            },
            {
                .name      = "ca",
                .t         = NEU_JSON_INT,
                .v.val_int = cert->ca,
            },
        };

        if (0 !=
            neu_json_encode_field(cert_object, cert_elems,
                                  NEU_JSON_ELEM_SIZE(cert_elems))) {
            neu_json_encode_free(cert_object);
            neu_json_encode_free(result_array);
            return -1;
        }

        if (0 != json_array_append_new(result_array, cert_object)) {
            neu_json_encode_free(cert_object);
            neu_json_encode_free(result_array);
            return -1;
        }
    }

    *json_obj = result_array;
    return 0;
}

// Authentication related functions
int neu_json_decode_auth_basic_enable_req(
    char *buf, neu_json_auth_basic_enable_req_t **result)
{
    int ret = 0;

    neu_json_auth_basic_enable_req_t *req =
        calloc(1, sizeof(neu_json_auth_basic_enable_req_t));
    neu_json_elem_t req_elems[] = { {
                                        .name = "node",
                                        .t    = NEU_JSON_STR,
                                    },
                                    {
                                        .name = "enabled",
                                        .t    = NEU_JSON_BOOL,
                                    } };

    ret = neu_json_decode(buf, NEU_JSON_ELEM_SIZE(req_elems), req_elems);
    if (ret != 0) {
        goto decode_fail;
    }

    req->app_name = req_elems[0].v.val_str;
    req->enabled  = req_elems[1].v.val_bool;

    *result = req;
    return ret;

decode_fail:
    free(req);
    return -1;
}

void neu_json_decode_auth_basic_enable_req_free(
    neu_json_auth_basic_enable_req_t *req)
{
    if (req) {
        if (req->app_name) {
            free(req->app_name);
        }
        free(req);
    }
}

int neu_json_decode_add_basic_user_req(char *                          buf,
                                       neu_json_add_basic_user_req_t **result)
{
    int ret = 0;

    neu_json_add_basic_user_req_t *req =
        calloc(1, sizeof(neu_json_add_basic_user_req_t));
    neu_json_elem_t req_elems[] = {
        {
            .name = "node",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "username",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "password",
            .t    = NEU_JSON_STR,
        },

    };
    ret = neu_json_decode(buf, NEU_JSON_ELEM_SIZE(req_elems), req_elems);
    if (ret != 0) {
        goto decode_fail;
    }

    req->app_name = req_elems[0].v.val_str;
    req->username = req_elems[1].v.val_str;
    req->password = req_elems[2].v.val_str;

    *result = req;
    return ret;

decode_fail:
    free(req);
    return -1;
}

void neu_json_decode_add_basic_user_req_free(neu_json_add_basic_user_req_t *req)
{
    if (req) {
        free(req->app_name);
        free(req->username);
        free(req->password);
        free(req);
    }
}

int neu_json_decode_update_basic_user_req(
    char *buf, neu_json_update_basic_user_req_t **result)
{
    int ret = 0;

    neu_json_update_basic_user_req_t *req =
        calloc(1, sizeof(neu_json_update_basic_user_req_t));
    neu_json_elem_t req_elems[] = {
        {
            .name = "node",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "username",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "password",
            .t    = NEU_JSON_STR,
        },

    };
    ret = neu_json_decode(buf, NEU_JSON_ELEM_SIZE(req_elems), req_elems);
    if (ret != 0) {
        goto decode_fail;
    }

    req->app_name     = req_elems[0].v.val_str;
    req->username     = req_elems[1].v.val_str;
    req->new_password = req_elems[2].v.val_str;

    *result = req;
    return ret;

decode_fail:
    free(req);
    return -1;
}

void neu_json_decode_update_basic_user_req_free(
    neu_json_update_basic_user_req_t *req)
{
    if (req) {
        free(req->app_name);
        free(req->username);
        free(req->new_password);
        free(req);
    }
}

int neu_json_decode_delete_basic_user_req(
    char *buf, neu_json_delete_basic_user_req_t **result)
{
    int                               ret = 0;
    neu_json_delete_basic_user_req_t *req =
        calloc(1, sizeof(neu_json_delete_basic_user_req_t));
    if (req == NULL) {
        return NEU_ERR_EINTERNAL;
    }

    neu_json_elem_t req_elems[] = {
        {
            .name = "node",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "username",
            .t    = NEU_JSON_STR,
        },
    };

    ret = neu_json_decode(buf, NEU_JSON_ELEM_SIZE(req_elems), req_elems);
    if (ret != 0) {
        goto decode_fail;
    }

    req->app_name = req_elems[0].v.val_str;
    req->username = req_elems[1].v.val_str;

    *result = req;
    return 0;

decode_fail:
    neu_json_decode_delete_basic_user_req_free(req);
    return ret;
}

void neu_json_decode_delete_basic_user_req_free(
    neu_json_delete_basic_user_req_t *req)
{
    if (req != NULL) {
        free(req->app_name);
        free(req->username);
        free(req);
    }
}

int neu_json_decode_get_basic_users_req(char *                           buf,
                                        neu_json_get_basic_users_req_t **result)
{
    int                             ret = 0;
    neu_json_get_basic_users_req_t *req =
        calloc(1, sizeof(neu_json_get_basic_users_req_t));
    if (req == NULL) {
        return NEU_ERR_EINTERNAL;
    }

    neu_json_elem_t req_elems[] = {
        {
            .name = "node",
            .t    = NEU_JSON_STR,
        },
    };

    ret = neu_json_decode(buf, NEU_JSON_ELEM_SIZE(req_elems), req_elems);
    if (ret != 0) {
        goto decode_fail;
    }

    req->app_name = req_elems[0].v.val_str;

    *result = req;
    return 0;

decode_fail:
    neu_json_decode_get_basic_users_req_free(req);
    return ret;
}

void neu_json_decode_get_basic_users_req_free(
    neu_json_get_basic_users_req_t *req)
{
    if (req != NULL) {
        free(req->app_name);
        free(req);
    }
}

int neu_json_encode_get_basic_users_resp(void *json_obj, void *param)
{
    neu_json_get_basic_users_resp_t *resp =
        (neu_json_get_basic_users_resp_t *) param;

    void *user_array = json_obj;

    for (int i = 0; i < resp->count; i++) {
        neu_json_basic_user_info_t *user        = resp->users[i];
        void *                      user_object = neu_json_encode_new();

        neu_json_elem_t user_elems[] = {
            {
                .name      = "username",
                .t         = NEU_JSON_STR,
                .v.val_str = user->username,
            },
            {
                .name      = "password",
                .t         = NEU_JSON_STR,
                .v.val_str = user->password_masked,
            },
        };

        if (0 !=
            neu_json_encode_field(user_object, user_elems,
                                  NEU_JSON_ELEM_SIZE(user_elems))) {
            neu_json_encode_free(user_object);
            return -1;
        }

        if (0 != json_array_append_new(user_array, user_object)) {
            neu_json_encode_free(user_object);
            return -1;
        }
    }

    return 0;
}

int neu_json_decode_auth_basic_status_req(
    char *buf, neu_json_auth_basic_status_req_t **result)
{
    int ret = 0;

    neu_json_auth_basic_status_req_t *req =
        calloc(1, sizeof(neu_json_auth_basic_status_req_t));
    if (req == NULL) {
        return NEU_ERR_EINTERNAL;
    }

    neu_json_elem_t req_elems[] = { {
        .name = "node",
        .t    = NEU_JSON_STR,
    } };

    ret = neu_json_decode(buf, NEU_JSON_ELEM_SIZE(req_elems), req_elems);
    if (ret != 0) {
        free(req);
        return ret;
    }

    req->app_name = req_elems[0].v.val_str;

    *result = req;
    return ret;
}

void neu_json_decode_auth_basic_status_req_free(
    neu_json_auth_basic_status_req_t *req)
{
    if (req) {
        if (req->app_name) {
            free(req->app_name);
        }
        free(req);
    }
}

int neu_json_encode_auth_basic_status_resp(void *json_obj, void *param)
{
    neu_json_auth_basic_status_resp_t *resp =
        (neu_json_auth_basic_status_resp_t *) param;

    neu_json_elem_t resp_elems[] = {
        {
            .name       = "enabled",
            .t          = NEU_JSON_BOOL,
            .v.val_bool = resp->enabled,
        },
    };

    if (0 !=
        neu_json_encode_field(json_obj, resp_elems,
                              NEU_JSON_ELEM_SIZE(resp_elems))) {
        return -1;
    }

    return 0;
}

int neu_json_decode_server_security_policy_req(
    char *buf, neu_json_server_security_policy_req_t **result)
{
    int                                    ret = 0;
    neu_json_server_security_policy_req_t *req =
        calloc(1, sizeof(neu_json_server_security_policy_req_t));
    if (req == NULL) {
        return NEU_ERR_EINTERNAL;
    }

    neu_json_elem_t req_elems[] = {
        {
            .name = "node",
            .t    = NEU_JSON_STR,
        },
        {
            .name = "policyName",
            .t    = NEU_JSON_ARRAY_STR,
        },
    };

    ret = neu_json_decode(buf, NEU_JSON_ELEM_SIZE(req_elems), req_elems);
    if (ret != 0) {
        goto decode_fail;
    }

    req->app_name     = req_elems[0].v.val_str;
    req->policy_names = req_elems[1].v.val_array_str.p_strs;
    req->policy_count = req_elems[1].v.val_array_str.length;

    *result = req;
    return 0;

decode_fail:
    neu_json_decode_server_security_policy_req_free(req);
    return ret;
}

void neu_json_decode_server_security_policy_req_free(
    neu_json_server_security_policy_req_t *req)
{
    if (req != NULL) {
        free(req->app_name);
        if (req->policy_names != NULL) {
            for (uint16_t i = 0; i < req->policy_count; i++) {
                free(req->policy_names[i]);
            }
            free(req->policy_names);
        }
        free(req);
    }
}

int neu_json_decode_get_security_policy_req(
    char *buf, neu_json_get_security_policy_req_t **result)
{
    int                                 ret = 0;
    neu_json_get_security_policy_req_t *req =
        calloc(1, sizeof(neu_json_get_security_policy_req_t));
    neu_json_elem_t req_elems[] = {
        {
            .name = "node",
            .t    = NEU_JSON_STR,
        },
    };

    if (req == NULL) {
        return NEU_ERR_EINTERNAL;
    }

    ret = neu_json_decode(buf, NEU_JSON_ELEM_SIZE(req_elems), req_elems);
    if (ret != 0) {
        neu_json_decode_get_security_policy_req_free(req);
        return ret;
    }

    req->app_name = req_elems[0].v.val_str;

    *result = req;
    return 0;
}

void neu_json_decode_get_security_policy_req_free(
    neu_json_get_security_policy_req_t *req)
{
    if (req != NULL) {
        free(req->app_name);
        free(req);
    }
}

int neu_json_encode_get_security_policy_resp(void *json_obj, void *param)
{
    neu_json_get_security_policy_resp_t *resp =
        (neu_json_get_security_policy_resp_t *) param;

    neu_json_elem_t resp_elems[] = {
        { .name                   = "policyName",
          .t                      = NEU_JSON_ARRAY_STR,
          .v.val_array_str.length = resp->policy_count,
          .v.val_array_str.p_strs = resp->policy_names },
    };

    return neu_json_encode_field(json_obj, resp_elems,
                                 NEU_JSON_ELEM_SIZE(resp_elems));
}
