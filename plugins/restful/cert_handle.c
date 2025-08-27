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
#include <string.h>

#include <nng/nng.h>
#include <nng/supplemental/http/http.h>

#include "define.h"
#include "errcodes.h"
#include "parser/neu_json_cert.h"
#include "parser/neu_json_login.h"
#include "plugin.h"
#include "user.h"
#include "utils/base64.h"
#include "utils/http.h"
#include "utils/log.h"
#include "json/neu_json_error.h"
#include "json/neu_json_fn.h"

#include "cert_handle.h"
#include "handle.h"

void handle_server_cert_self_sign(nng_aio *aio)
{
    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_server_cert_self_sign_req_t,
        neu_json_decode_server_cert_self_sign_req, {
            nlog_info("Server certificate self-sign request: app_name=%s, "
                      "days_valid=%d",
                      req->app_name, req->days_valid);

            neu_reqresp_head_t header = { 0 };

            header.ctx             = aio;
            header.type            = NEU_REQ_SERVER_CERT_SELF_SIGN;
            header.otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM;

            neu_req_server_cert_self_sign_t req_msg = { 0 };
            req_msg.days_valid                      = req->days_valid;
            snprintf(req_msg.app_name, sizeof(req_msg.app_name), "%s",
                     req->app_name);
            int ret = neu_plugin_op(neu_rest_get_plugin(), header, &req_msg);
            if (ret != 0) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                    neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
                });
            }
        })
}

void handle_server_cert_upload(nng_aio *aio)
{
    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_server_cert_upload_req_t,
        neu_json_decode_server_cert_upload_req, {
            nlog_info("Server certificate upload request: app_name=%s",
                      req->app_name);

            neu_reqresp_head_t header = { 0 };

            header.ctx             = aio;
            header.type            = NEU_REQ_SERVER_CERT_UPLOAD;
            header.otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM;

            neu_req_server_cert_upload_t req_msg = { 0 };
            snprintf(req_msg.app_name, sizeof(req_msg.app_name), "%s",
                     req->app_name);
            req_msg.cert_base64 = strdup(req->certificate);
            req_msg.key_base64  = strdup(req->private_key);

            int ret = neu_plugin_op(neu_rest_get_plugin(), header, &req_msg);
            if (ret != 0) {
                free(req_msg.cert_base64);
                free(req_msg.key_base64);
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                    neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
                });
            }
        })
}

void handle_server_cert_get(nng_aio *aio)
{
    char app_name[NEU_NODE_NAME_LEN] = { 0 };

    NEU_VALIDATE_JWT(aio);

    // Extract app_name from query parameters
    if (neu_http_get_param_str(aio, "node", app_name, sizeof(app_name)) <= 0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_PARAM_IS_WRONG, {
            neu_http_response(aio, NEU_ERR_PARAM_IS_WRONG, result_error);
        });
        return;
    }

    nlog_info("Server certificate get request: app_name=%s", app_name);

    neu_reqresp_head_t header = { 0 };
    header.ctx                = aio;
    header.type               = NEU_REQ_SERVER_CERT_INFO;
    header.otel_trace_type    = NEU_OTEL_TRACE_TYPE_REST_COMM;

    neu_req_server_cert_data_t req_msg = { 0 };
    snprintf(req_msg.app_name, sizeof(req_msg.app_name), "%s", app_name);

    int ret = neu_plugin_op(neu_rest_get_plugin(), header, &req_msg);
    if (ret != 0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
            neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
        });
    }
}

void handle_server_cert_get_resp(nng_aio *                    aio,
                                 neu_resp_server_cert_data_t *resp)
{
    char *                           result   = NULL;
    neu_json_server_cert_info_resp_t resp_obj = { 0 };

    // Convert response data to JSON response structure
    resp_obj.subject     = resp->subject;
    resp_obj.fingerprint = resp->fingerprint;
    resp_obj.issuer      = resp->issuer;
    resp_obj.valid_to    = resp->expire;

    if (neu_json_encode_by_fn(&resp_obj, neu_json_encode_server_cert_info_resp,
                              &result) != 0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_EINTERNAL, {
            neu_http_response(aio, NEU_ERR_EINTERNAL, result_error);
        });
    } else {
        neu_http_ok(aio, result);
        free(result);
    }

    // Free response data
    if (resp->subject)
        free(resp->subject);
    if (resp->issuer)
        free(resp->issuer);
    if (resp->expire)
        free(resp->expire);
    if (resp->fingerprint)
        free(resp->fingerprint);
}

void handle_server_cert_export(nng_aio *aio)
{
    char app_name[NEU_NODE_NAME_LEN] = { 0 };

    NEU_VALIDATE_JWT(aio);

    // Extract app_name from query parameters
    if (neu_http_get_param_str(aio, "node", app_name, sizeof(app_name)) <= 0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_PARAM_IS_WRONG, {
            neu_http_response(aio, NEU_ERR_PARAM_IS_WRONG, result_error);
        });
        return;
    }

    nlog_info("Server certificate export request: app_name=%s", app_name);

    neu_reqresp_head_t header = { 0 };
    header.ctx                = aio;
    header.type               = NEU_REQ_SERVER_CERT_EXPORT;
    header.otel_trace_type    = NEU_OTEL_TRACE_TYPE_REST_COMM;

    neu_req_server_cert_export_t req_msg = { 0 };
    snprintf(req_msg.app_name, sizeof(req_msg.app_name), "%s", app_name);

    int ret = neu_plugin_op(neu_rest_get_plugin(), header, &req_msg);
    if (ret != 0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
            neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
        });
    }
}

void handle_server_cert_export_resp(nng_aio *                      aio,
                                    neu_resp_server_cert_export_t *resp)
{
    if (resp->cert_base64 == NULL) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_EINTERNAL, {
            neu_http_response(aio, NEU_ERR_EINTERNAL, result_error);
        });
        return;
    }

    // Decode base64 certificate data
    int            cert_len  = 0;
    unsigned char *cert_data = neu_decode64(&cert_len, resp->cert_base64);
    if (cert_data == NULL) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_EINTERNAL, {
            neu_http_response(aio, NEU_ERR_EINTERNAL, result_error);
        });
        free(resp->cert_base64);
        return;
    }

    // Create HTTP response
    nng_http_res *res = NULL;
    nng_http_res_alloc(&res);

    // Set response headers for file download
    nng_http_res_set_header(res, "Content-Type", "application/x-pem-file");
    nng_http_res_set_header(res, "Content-Disposition",
                            "attachment; filename=\"server_cert.pem\"");
    nng_http_res_set_header(res, "Access-Control-Allow-Origin", "*");
    nng_http_res_set_header(res, "Access-Control-Allow-Methods",
                            "POST,GET,PUT,DELETE,OPTIONS");
    nng_http_res_set_header(res, "Access-Control-Allow-Headers", "*");

    // Set response body with certificate data
    nng_http_res_copy_data(res, (char *) cert_data, cert_len);
    nng_http_res_set_status(res, NNG_HTTP_STATUS_OK);

    nng_http_req *nng_req = nng_aio_get_input(aio, 0);
    nlog_notice("<%p> %s %s [%d]", aio, nng_http_req_get_method(nng_req),
                nng_http_req_get_uri(nng_req), NNG_HTTP_STATUS_OK);

    nng_aio_set_output(aio, 0, res);
    nng_aio_finish(aio, 0);

    // Clean up
    free(cert_data);
    free(resp->cert_base64);
}

void handle_client_cert_upload(nng_aio *aio)
{
    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_client_cert_upload_req_t,
        neu_json_decode_client_cert_upload_req, {
            nlog_info("Client certificate upload request: app_name=%s, "
                      "is_ca=%d",
                      req->app_name, req->is_ca);

            neu_reqresp_head_t header = { 0 };

            header.ctx             = aio;
            header.type            = NEU_REQ_CLIENT_CERT_UPLOAD;
            header.otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM;

            neu_req_client_cert_upload_t req_msg = { 0 };
            snprintf(req_msg.app_name, sizeof(req_msg.app_name), "%s",
                     req->app_name);
            req_msg.cert_base64 = strdup(req->certificate);
            req_msg.ca          = req->is_ca;

            int ret = neu_plugin_op(neu_rest_get_plugin(), header, &req_msg);
            if (ret != 0) {
                free(req_msg.cert_base64);
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                    neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
                });
            }
        })
}

void handle_client_cert_delete(nng_aio *aio)
{
    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_client_cert_delete_req_t,
        neu_json_decode_client_cert_delete_req, {
            nlog_info("Client certificate delete request: app_name=%s, "
                      "fingerprint=%s",
                      req->app_name, req->fingerprint);

            neu_reqresp_head_t header = { 0 };

            header.ctx             = aio;
            header.type            = NEU_REQ_CLIENT_CERT_DELETE;
            header.otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM;

            neu_req_client_cert_del_t req_msg = { 0 };
            snprintf(req_msg.app_name, sizeof(req_msg.app_name), "%s",
                     req->app_name);
            req_msg.fingerprint = strdup(req->fingerprint);

            int ret = neu_plugin_op(neu_rest_get_plugin(), header, &req_msg);
            if (ret != 0) {
                free(req_msg.fingerprint);
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                    neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
                });
            }
        })
}

void handle_client_cert_trust(nng_aio *aio)
{
    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_client_cert_trust_req_t,
        neu_json_decode_client_cert_trust_req, {
            nlog_info("Client certificate trust request: app_name=%s, "
                      "fingerprint=%s",
                      req->app_name, req->fingerprint);

            neu_reqresp_head_t header = { 0 };

            header.ctx             = aio;
            header.type            = NEU_REQ_CLIENT_CERT_TRUST;
            header.otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM;

            neu_req_client_cert_trust_t req_msg = { 0 };
            snprintf(req_msg.app_name, sizeof(req_msg.app_name), "%s",
                     req->app_name);
            req_msg.fingerprint = strdup(req->fingerprint);

            int ret = neu_plugin_op(neu_rest_get_plugin(), header, &req_msg);
            if (ret != 0) {
                free(req_msg.fingerprint);
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                    neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
                });
            }
        })
}

void handle_client_cert_get(nng_aio *aio)
{
    char app_name[NEU_NODE_NAME_LEN] = { 0 };
    char is_ca_str[10]               = { 0 };
    int  is_ca                       = 0;

    NEU_VALIDATE_JWT(aio);

    // Extract app_name from query parameters
    if (neu_http_get_param_str(aio, "node", app_name, sizeof(app_name)) <= 0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_PARAM_IS_WRONG, {
            neu_http_response(aio, NEU_ERR_PARAM_IS_WRONG, result_error);
        });
        return;
    }

    // Extract is_ca from query parameters (optional, default to 0)
    if (neu_http_get_param_str(aio, "isCa", is_ca_str, sizeof(is_ca_str)) > 0) {
        is_ca = atoi(is_ca_str);
    }

    nlog_info("Client certificate get request: app_name=%s, is_ca=%d", app_name,
              is_ca);

    neu_reqresp_head_t header = { 0 };
    header.ctx                = aio;
    header.type               = NEU_REQ_CLIENT_CERT_INFO;
    header.otel_trace_type    = NEU_OTEL_TRACE_TYPE_REST_COMM;

    neu_req_client_cert_data_t req_msg = { 0 };
    snprintf(req_msg.app_name, sizeof(req_msg.app_name), "%s", app_name);
    req_msg.ca = is_ca;

    int ret = neu_plugin_op(neu_rest_get_plugin(), header, &req_msg);
    if (ret != 0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
            neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
        });
    }
}

void handle_client_cert_get_resp(nng_aio *                     aio,
                                 neu_resp_client_certs_data_t *resp)
{
    char *                           result     = NULL;
    neu_json_client_cert_info_resp_t resp_obj   = { 0 };
    neu_json_client_cert_info_t **   certs      = NULL;
    int                              cert_count = 0;

    // Calculate number of certificates
    if (resp->certs) {
        cert_count = utarray_len(resp->certs);
    }

    if (cert_count > 0) {
        certs = malloc(cert_count * sizeof(neu_json_client_cert_info_t *));
        if (!certs) {
            NEU_JSON_RESPONSE_ERROR(NEU_ERR_EINTERNAL, {
                neu_http_response(aio, NEU_ERR_EINTERNAL, result_error);
            });
            goto cleanup;
        }

        int i = 0;
        utarray_foreach(resp->certs, neu_resp_client_cert_data_t *, cert_data)
        {
            certs[i] = malloc(sizeof(neu_json_client_cert_info_t));
            if (!certs[i]) {
                // Free previously allocated certificates
                for (int j = 0; j < i; j++) {
                    free(certs[j]);
                }
                free(certs);
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_EINTERNAL, {
                    neu_http_response(aio, NEU_ERR_EINTERNAL, result_error);
                });
                goto cleanup;
            }

            // Map response data to JSON structure
            // Note: According to API spec, certificate field should contain the
            // certificate string For now, using subject as placeholder until
            // certificate data is available from backend
            certs[i]->subject =
                cert_data->subject ? strdup(cert_data->subject) : strdup("");
            certs[i]->fingerprint = cert_data->fingerprint
                ? strdup(cert_data->fingerprint)
                : strdup("");
            certs[i]->issuer =
                cert_data->issuer ? strdup(cert_data->issuer) : strdup("");
            certs[i]->valid_to =
                cert_data->expire ? strdup(cert_data->expire) : strdup("");
            certs[i]->trust_status = cert_data->trust_status;
            certs[i]->ca           = cert_data->ca;

            i++;
        }
    }

    resp_obj.certs = certs;
    resp_obj.count = cert_count;

    void *obj_array = NULL;

    if (0 != neu_json_encode_client_cert_info_resp(&obj_array, &resp_obj)) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_EINTERNAL, {
            neu_http_response(aio, NEU_ERR_EINTERNAL, result_error);
        });
        goto cleanup;
    } else {
        int ret = neu_json_encode(obj_array, &result);
        neu_json_decode_free(obj_array);
        if (ret != 0) {
            NEU_JSON_RESPONSE_ERROR(NEU_ERR_EINTERNAL, {
                neu_http_response(aio, NEU_ERR_EINTERNAL, result_error);
            });
            goto cleanup;
        }
        neu_http_ok(aio, result);
        free(result);
    }

cleanup:
    if (certs) {
        for (int i = 0; i < cert_count; i++) {
            if (certs[i]) {
                free(certs[i]->subject);
                free(certs[i]->fingerprint);
                free(certs[i]->issuer);
                free(certs[i]->valid_to);
                free(certs[i]);
            }
        }
        free(certs);
    }
}

// Authentication related functions
void handle_auth_basic_enable(nng_aio *aio)
{
    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_auth_basic_enable_req_t,
        neu_json_decode_auth_basic_enable_req, {
            neu_plugin_t *               plugin = neu_rest_get_plugin();
            neu_reqresp_head_t           header = { 0 };
            neu_req_server_auth_switch_t cmd    = { 0 };
            int                          ret;

            header.ctx             = aio;
            header.type            = NEU_REQ_SERVER_AUTH_SWITCH;
            header.otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM;

            // Copy app_name to fixed-size buffer
            strncpy(cmd.app_name, req->app_name, sizeof(cmd.app_name) - 1);
            cmd.app_name[sizeof(cmd.app_name) - 1] = '\0';
            cmd.enable                             = req->enabled;

            ret = neu_plugin_op(plugin, header, &cmd);
            if (ret != 0) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                    neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
                });
            }
        })
}

void handle_auth_basic_status(nng_aio *aio)
{
    char app_name[NEU_NODE_NAME_LEN] = { 0 };

    NEU_VALIDATE_JWT(aio);

    // Extract app_name from query parameters
    if (neu_http_get_param_str(aio, "node", app_name, sizeof(app_name)) <= 0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_PARAM_IS_WRONG, {
            neu_http_response(aio, NEU_ERR_PARAM_IS_WRONG, result_error);
        });
        return;
    }

    nlog_info("Auth basic status request: app_name=%s", app_name);

    neu_plugin_t *                      plugin = neu_rest_get_plugin();
    neu_reqresp_head_t                  header = { 0 };
    neu_req_server_auth_switch_status_t cmd    = { 0 };
    int                                 ret;

    header.ctx             = aio;
    header.type            = NEU_REQ_SERVER_AUTH_SWITCH_STATUS;
    header.otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM;

    // Copy app_name to fixed-size buffer
    strncpy(cmd.app_name, app_name, sizeof(cmd.app_name) - 1);
    cmd.app_name[sizeof(cmd.app_name) - 1] = '\0';

    ret = neu_plugin_op(plugin, header, &cmd);
    if (ret != 0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
            neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
        });
    }
}

void handle_auth_basic_status_resp(nng_aio *                             aio,
                                   neu_resp_server_auth_switch_status_t *resp)
{
    neu_json_auth_basic_status_resp_t json_resp = { 0 };
    json_resp.enabled                           = resp->enable;

    char *result = NULL;
    neu_json_encode_by_fn(&json_resp, neu_json_encode_auth_basic_status_resp,
                          &result);

    neu_http_ok(aio, result);
    free(result);
}

void handle_add_basic_user(nng_aio *aio)
{
    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_add_basic_user_req_t, neu_json_decode_add_basic_user_req,
        {
            // Forward message to core using NEU_REQ_SERVER_AUTH_USER_ADD

            neu_plugin_t *                 plugin = neu_rest_get_plugin();
            neu_reqresp_head_t             header = { 0 };
            neu_req_server_auth_user_add_t cmd    = { 0 };
            int                            ret;

            header.ctx             = aio;
            header.type            = NEU_REQ_SERVER_AUTH_USER_ADD;
            header.otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM;

            // Copy app_name to fixed-size buffer
            strncpy(cmd.app_name, req->app_name, sizeof(cmd.app_name) - 1);
            cmd.app_name[sizeof(cmd.app_name) - 1] = '\0';

            // Allocate and copy username and password
            cmd.user = strdup(req->username);
            cmd.pwd  = strdup(req->password);

            if (!cmd.user || !cmd.pwd) {
                nlog_error("Failed to allocate memory for user credentials");
                free(cmd.user);
                free(cmd.pwd);
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_EINTERNAL, {
                    neu_http_response(aio, error_code.error, result_error);
                });
                return;
            }

            ret = neu_plugin_op(plugin, header, &cmd);
            if (ret != 0) {
                nlog_error("Failed to send auth user add request to core");
                free(cmd.user);
                free(cmd.pwd);
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_EINTERNAL, {
                    neu_http_response(aio, error_code.error, result_error);
                });
                return;
            }
        })
}

void handle_update_basic_user(nng_aio *aio)
{
    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_update_basic_user_req_t,
        neu_json_decode_update_basic_user_req, {
            // Forward message to core using NEU_REQ_SERVER_AUTH_USER_UPDATE_PWD

            neu_plugin_t *                    plugin = neu_rest_get_plugin();
            neu_reqresp_head_t                header = { 0 };
            neu_req_server_auth_user_update_t cmd    = { 0 };
            int                               ret;

            header.ctx             = aio;
            header.type            = NEU_REQ_SERVER_AUTH_USER_UPDATE_PWD;
            header.otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM;

            // Copy app_name to fixed-size buffer
            strncpy(cmd.app_name, req->app_name, sizeof(cmd.app_name) - 1);
            cmd.app_name[sizeof(cmd.app_name) - 1] = '\0';

            // Allocate and copy username and new password
            cmd.user    = strdup(req->username);
            cmd.new_pwd = strdup(req->new_password);
            cmd.pwd     = NULL;

            if (!cmd.user || !cmd.new_pwd) {
                nlog_error("Failed to allocate memory for user credentials");
                free(cmd.user);
                free(cmd.new_pwd);
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_EINTERNAL, {
                    neu_http_response(aio, error_code.error, result_error);
                });
                return;
            }

            ret = neu_plugin_op(plugin, header, &cmd);
            if (ret != 0) {
                nlog_error("Failed to send auth user update request to core");
                free(cmd.user);
                free(cmd.new_pwd);
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_EINTERNAL, {
                    neu_http_response(aio, error_code.error, result_error);
                });
                return;
            }
        })
}

void handle_delete_basic_user(nng_aio *aio)
{
    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_delete_basic_user_req_t,
        neu_json_decode_delete_basic_user_req, {
            // Forward message to core using NEU_REQ_SERVER_AUTH_USER_DELETE

            neu_plugin_t *                 plugin = neu_rest_get_plugin();
            neu_reqresp_head_t             header = { 0 };
            neu_req_server_auth_user_del_t cmd    = { 0 };
            int                            ret;

            header.ctx             = aio;
            header.type            = NEU_REQ_SERVER_AUTH_USER_DELETE;
            header.otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM;

            // Copy app_name to fixed-size buffer
            strncpy(cmd.app_name, req->app_name, sizeof(cmd.app_name) - 1);
            cmd.app_name[sizeof(cmd.app_name) - 1] = '\0';

            // Allocate and copy username
            cmd.user = strdup(req->username);

            if (!cmd.user) {
                nlog_error("Failed to allocate memory for username");
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_EINTERNAL, {
                    neu_http_response(aio, error_code.error, result_error);
                });
                return;
            }

            ret = neu_plugin_op(plugin, header, &cmd);
            if (ret != 0) {
                nlog_error("Failed to send auth user delete request to core");
                free(cmd.user);
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_EINTERNAL, {
                    neu_http_response(aio, error_code.error, result_error);
                });
                return;
            }
        })
}

void handle_get_basic_users(nng_aio *aio)
{
    char    app_name_buf[NEU_NODE_NAME_LEN] = { 0 };
    ssize_t result =
        neu_http_get_param_str(aio, "node", app_name_buf, sizeof(app_name_buf));
    if (result < 0) {
        nlog_error("Missing or invalid required parameter: app_name");
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_PARAM_IS_WRONG, {
            neu_http_response(aio, NEU_ERR_PARAM_IS_WRONG, result_error);
        });
        return;
    }

    // Forward message to core using NEU_REQ_SERVER_AUTH_USER_INFO
    neu_plugin_t *                   plugin = neu_rest_get_plugin();
    neu_reqresp_head_t               header = { 0 };
    neu_req_server_auth_users_info_t cmd    = { 0 };
    int                              ret;

    header.ctx             = aio;
    header.type            = NEU_REQ_SERVER_AUTH_USER_INFO;
    header.otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM;

    // Copy app_name to fixed-size buffer
    strncpy(cmd.app_name, app_name_buf, sizeof(cmd.app_name) - 1);
    cmd.app_name[sizeof(cmd.app_name) - 1] = '\0';

    ret = neu_plugin_op(plugin, header, &cmd);
    if (ret != 0) {
        nlog_error("Failed to send auth user info request to core");
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_EINTERNAL, {
            neu_http_response(aio, error_code.error, result_error);
        });
        return;
    }

    // The response will be handled by handle_get_basic_users_resp
}

void handle_get_basic_users_resp(nng_aio *                          aio,
                                 neu_resp_server_auth_users_info_t *resp)
{
    neu_json_get_basic_users_resp_t json_resp = { 0 };

    if (resp->users) {
        // Count users
        json_resp.count = utarray_len(resp->users);

        // Allocate array for user info
        json_resp.users =
            calloc(json_resp.count, sizeof(neu_json_basic_user_info_t *));
        if (!json_resp.users) {
            nlog_error("Failed to allocate memory for user list");
            NEU_JSON_RESPONSE_ERROR(NEU_ERR_EINTERNAL, {
                neu_http_response(aio, error_code.error, result_error);
            });
            return;
        }

        // Convert core message users to JSON format
        neu_server_auth_user_t *p = NULL;
        int                     i = 0;

        while ((p = (neu_server_auth_user_t *) utarray_next(resp->users, p))) {
            neu_json_basic_user_info_t *user_info =
                calloc(1, sizeof(neu_json_basic_user_info_t));

            user_info->username        = strdup(p->user);
            user_info->password_masked = strdup(p->pwd_mask);

            json_resp.users[i] = user_info;
            i++;
        }
    } else {
        json_resp.count = 0;
        json_resp.users = NULL;
    }

    char *result = NULL;

    void *object_array = neu_json_array();

    neu_json_encode_get_basic_users_resp(object_array, &json_resp);
    neu_json_encode(object_array, &result);
    neu_json_decode_free(object_array);

    // Free allocated memory
    for (int i = 0; i < json_resp.count; i++) {
        free(json_resp.users[i]->username);
        free(json_resp.users[i]->password_masked);
        free(json_resp.users[i]);
    }
    free(json_resp.users);

    neu_http_ok(aio, result);
    free(result);
}

// Security policy related functions
void handle_server_security_policy(nng_aio *aio)
{
    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_server_security_policy_req_t,
        neu_json_decode_server_security_policy_req, {
            char policy_name[256] = { 0 };

            for (int i = 0; i < req->policy_count; i++) {
                strncat(policy_name, req->policy_names[i],
                        sizeof(policy_name) - strlen(policy_name) - 1);
                if (i < req->policy_count - 1) {
                    strncat(policy_name, ",",
                            sizeof(policy_name) - strlen(policy_name) - 1);
                }
            }

            nlog_info("Server security policy request: app_name=%s, "
                      "policy_name=%s",
                      req->app_name, policy_name);

            neu_reqresp_head_t header = { 0 };

            header.ctx             = aio;
            header.type            = NEU_REQ_SERVER_SECURITY_POLICY;
            header.otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM;

            neu_req_server_security_policy_t req_msg = { 0 };
            snprintf(req_msg.app_name, sizeof(req_msg.app_name), "%s",
                     req->app_name);
            req_msg.policy_name = strdup(policy_name);

            int ret = neu_plugin_op(neu_rest_get_plugin(), header, &req_msg);
            if (ret != 0) {
                free(req_msg.policy_name);
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
                    neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
                });
            }
        })
}

void handle_get_server_security_policy(nng_aio *aio)
{
    neu_plugin_t *plugin                      = neu_rest_get_plugin();
    char          app_name[NEU_NODE_NAME_LEN] = { 0 };

    if (neu_http_get_param_str(aio, "node", app_name, sizeof(app_name)) <= 0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_PARAM_IS_WRONG, {
            neu_http_response(aio, NEU_ERR_PARAM_IS_WRONG, result_error);
        });
        return;
    }

    nlog_info("Get server security policy request: app_name=%s", app_name);

    neu_reqresp_head_t header = { 0 };
    header.ctx                = aio;
    header.type               = NEU_REQ_SERVER_SECURITY_POLICY_STATUS;
    header.otel_trace_type    = NEU_OTEL_TRACE_TYPE_REST_COMM;

    neu_req_server_security_policy_status_t req_msg = { 0 };
    snprintf(req_msg.app_name, sizeof(req_msg.app_name), "%s", app_name);

    int ret = neu_plugin_op(plugin, header, &req_msg);
    if (ret != 0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
            neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
        });
    }
}

void handle_get_server_security_policy_resp(
    nng_aio *aio, neu_resp_server_security_policy_status_t *resp)
{
    neu_json_get_security_policy_resp_t json_resp = { 0 };
    char *                              result    = NULL;

    char *policy_names[32] = { 0 };
    int   policy_count     = 0;

    if (resp->policy_name != NULL && strlen(resp->policy_name) > 0) {
        // Split policy names by comma
        char *token = strtok(resp->policy_name, ",");
        while (token != NULL) {
            policy_names[policy_count] = strdup(token);
            policy_count++;
            token = strtok(NULL, ",");
        }
    }

    json_resp.policy_names = malloc(policy_count * sizeof(char *));

    for (int i = 0; i < policy_count; i++) {
        json_resp.policy_names[i] = policy_names[i];
    }

    json_resp.policy_count = policy_count;

    if (neu_json_encode_by_fn(&json_resp,
                              neu_json_encode_get_security_policy_resp,
                              &result) != 0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_EINTERNAL, {
            neu_http_response(aio, NEU_ERR_EINTERNAL, result_error);
        });

        // Free allocated policy names
        for (int i = 0; i < policy_count; i++) {
            free(policy_names[i]);
        }

        free(json_resp.policy_names);

        return;
    }

    neu_http_ok(aio, result);
    free(result);

    for (int i = 0; i < policy_count; i++) {
        free(policy_names[i]);
    }

    free(json_resp.policy_names);
}
