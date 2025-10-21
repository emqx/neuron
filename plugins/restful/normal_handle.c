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

#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <jwt.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "persist/persist.h"
#include "user.h"
#include "utils/asprintf.h"
#include "utils/log.h"

#include "argparse.h"
#include "parser/neu_json_login.h"
#include "persist/persist_impl.h"
#include "json/neu_json_fn.h"

#include "handle.h"
#include "utils/http.h"
#include "utils/neu_jwt.h"

#include "normal_handle.h"

void handle_ping(nng_aio *aio)
{
    neu_http_ok(aio, "{}");
}

void handle_login(nng_aio *aio)
{
    NEU_PROCESS_HTTP_REQUEST(
        aio, neu_json_login_req_t, neu_json_decode_login_req, {
            neu_json_login_resp_t login_resp = { 0 };
            neu_user_t *          user       = neu_load_user(req->name);
            int                   pass_len   = strlen(req->pass);

            if (NULL == user) {
                nlog_error("could not find user `%s`", req->name);
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_INVALID_USER_OR_PASSWORD, {
                    neu_http_response(aio, error_code.error, result_error);
                });
            } else if (pass_len < NEU_USER_PASSWORD_MIN_LEN ||
                       pass_len > NEU_USER_PASSWORD_MAX_LEN) {
                nlog_error("user `%s` password too short or too long",
                           req->name);
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_INVALID_PASSWORD_LEN, {
                    neu_http_response(aio, error_code.error, result_error);
                });
            } else if (neu_user_check_password(user, req->pass)) {

                char *token  = NULL;
                char *result = NULL;

                int ret = neu_jwt_new(&token, req->name);
                if (ret != 0) {
                    NEU_JSON_RESPONSE_ERROR(NEU_ERR_NEED_TOKEN, {
                        neu_http_response(aio, error_code.error, result_error);
                        jwt_free_str(token);
                    });
                }

                login_resp.token = token;

                neu_json_encode_by_fn(&login_resp, neu_json_encode_login_resp,
                                      &result);
                neu_http_ok(aio, result);
                jwt_free_str(token);
                free(result);
            } else {
                nlog_error("user `%s` password check fail", req->name);
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_INVALID_USER_OR_PASSWORD, {
                    neu_http_response(aio, error_code.error, result_error);
                });
            }

            neu_user_free(user);
        })
}

void handle_get_user(nng_aio *aio)
{
    char current_user[NEU_USER_NAME_MAX_LEN + 1] = { 0 };
    NEU_VALIDATE_JWT_WITH_USER(aio, current_user);

    UT_icd    icd       = { sizeof(neu_json_user_resp_t), NULL, NULL, NULL };
    UT_array *user_list = NULL;
    utarray_new(user_list, &icd);

    UT_array *user_infos = neu_user_list();
    utarray_foreach(user_infos, neu_persist_user_info_t *, p_info)
    {
        neu_json_user_resp_t resp = { 0 };
        strcpy(resp.name, p_info->name);
        utarray_push_back(user_list, &resp);
    }
    utarray_free(user_infos);

    char *result = NULL;
    neu_json_encode_by_fn(user_list, neu_json_encode_user_list_resp, &result);
    neu_http_ok(aio, result);
    free(result);
    utarray_free(user_list);
}

void handle_add_user(nng_aio *aio)
{
    char current_user[NEU_USER_NAME_MAX_LEN + 1] = { 0 };
    NEU_VALIDATE_JWT_WITH_USER(aio, current_user);
    NEU_PROCESS_HTTP_REQUEST(
        aio, neu_json_add_user_req_t, neu_json_decode_add_user_req, {
            // user name length check
            int name_len = strlen(req->name);
            if (name_len < NEU_USER_NAME_MIN_LEN ||
                name_len > NEU_USER_NAME_MAX_LEN) {
                nlog_error("user name too short or too long");
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_INVALID_USER_LEN, {
                    neu_http_response(aio, error_code.error, result_error);
                });
                return;
            }

            // user password length check
            int pass_len = strlen(req->pass);
            if (pass_len < NEU_USER_PASSWORD_MIN_LEN ||
                pass_len > NEU_USER_PASSWORD_MAX_LEN) {
                nlog_error("user `%s` password too short or too long",
                           req->name);
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_INVALID_PASSWORD_LEN, {
                    neu_http_response(aio, error_code.error, result_error);
                });
                return;
            }

            // user already exists
            neu_user_t *user = neu_load_user(req->name);
            if (NULL != user) {
                nlog_error("user `%s` already exists", req->name);
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_USER_ALREADY_EXISTS, {
                    neu_http_response(aio, error_code.error, result_error);
                });

                neu_user_free(user);
                return;
            }

            // add user
            if (0 != neu_user_add(req->name, req->pass)) {
                nlog_error("add user `%s` fail", req->name);
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_EINTERNAL, {
                    neu_http_response(aio, error_code.error, result_error);
                });
                return;
            }

            NEU_JSON_RESPONSE_ERROR(NEU_ERR_SUCCESS, {
                neu_http_response(aio, error_code.error, result_error);
            });
        })
}

void handle_update_user(nng_aio *aio)
{
    char current_user[NEU_USER_NAME_MAX_LEN + 1] = { 0 };
    NEU_VALIDATE_JWT_WITH_USER(aio, current_user);
    NEU_PROCESS_HTTP_REQUEST(
        aio, neu_json_password_req_t, neu_json_decode_update_user_req, {
            // user name length check
            int name_len = strlen(req->name);
            if (name_len < NEU_USER_NAME_MIN_LEN ||
                name_len > NEU_USER_NAME_MAX_LEN) {
                nlog_error("user name too short or too long");
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_INVALID_USER_LEN, {
                    neu_http_response(aio, error_code.error, result_error);
                });
                return;
            }

            // new password length check
            int new_pass_len = strlen(req->new_pass);
            if (new_pass_len < NEU_USER_PASSWORD_MIN_LEN ||
                new_pass_len > NEU_USER_PASSWORD_MAX_LEN) {
                nlog_error("user `%s` new password too short or too long",
                           req->name);
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_INVALID_PASSWORD_LEN, {
                    neu_http_response(aio, error_code.error, result_error);
                });
                return;
            }

            // only admin & current user can update user
            if (0 != strcmp("admin", current_user) &&
                0 != strcmp(req->name, current_user)) {
                nlog_error("only admin & current user can update user");
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_USER_NO_PERMISSION, {
                    neu_http_response(aio, error_code.error, result_error);
                });
                return;
            }

            // user not exists
            neu_user_t *user = neu_load_user(req->name);
            if (NULL == user) {
                nlog_error("user `%s` not exists", req->name);
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_USER_NOT_EXISTS, {
                    neu_http_response(aio, error_code.error, result_error);
                });
                return;
            }

            // update user password
            int rv = neu_user_update_password(user, req->new_pass);
            if (0 != rv) {
                nlog_error("update user `%s` fail", req->name);
                NEU_JSON_RESPONSE_ERROR(rv, {
                    neu_http_response(aio, error_code.error, result_error);
                });

                neu_user_free(user);
                return;
            }

            // save user
            rv = neu_save_user(user);
            if (0 != rv) {
                nlog_error("update user `%s` fail", req->name);
                NEU_JSON_RESPONSE_ERROR(rv, {
                    neu_http_response(aio, error_code.error, result_error);
                });

                neu_user_free(user);
                return;
            }

            NEU_JSON_RESPONSE_ERROR(NEU_ERR_SUCCESS, {
                neu_http_response(aio, error_code.error, result_error);
            });

            neu_user_free(user);
        })
}

void handle_delete_user(nng_aio *aio)
{
    char current_user[NEU_USER_NAME_MAX_LEN + 1] = { 0 };
    NEU_VALIDATE_JWT_WITH_USER(aio, current_user);
    NEU_PROCESS_HTTP_REQUEST(
        aio, neu_json_delete_user_req_t, neu_json_decode_delete_user_req, {
            // user name length check
            int name_len = strlen(req->name);
            if (name_len < NEU_USER_NAME_MIN_LEN ||
                name_len > NEU_USER_NAME_MAX_LEN) {
                nlog_error("user name too short or too long");
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_INVALID_USER_LEN, {
                    neu_http_response(aio, error_code.error, result_error);
                });
                return;
            }

            // only admin can delete user
            if (0 != strcmp(current_user, "admin")) {
                nlog_error("only admin can delete user");
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_USER_NO_PERMISSION, {
                    neu_http_response(aio, error_code.error, result_error);
                });
                return;
            }

            // admin can not be deleted
            if (0 == strcmp(req->name, "admin")) {
                nlog_error("admin can not be deleted");
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_USER_NO_PERMISSION, {
                    neu_http_response(aio, error_code.error, result_error);
                });
                return;
            }

            // user not exists
            neu_user_t *user = neu_load_user(req->name);
            if (NULL == user) {
                nlog_error("user `%s` not exists", req->name);
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_USER_NOT_EXISTS, {
                    neu_http_response(aio, error_code.error, result_error);
                });

                return;
            }

            // delete user
            if (0 != neu_user_delete(req->name)) {
                nlog_error("delete user `%s` fail", req->name);
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_EINTERNAL, {
                    neu_http_response(aio, error_code.error, result_error);
                });

                neu_user_free(user);
                return;
            }

            NEU_JSON_RESPONSE_ERROR(NEU_ERR_SUCCESS, {
                neu_http_response(aio, error_code.error, result_error);
            });

            neu_user_free(user);
        })
}

void handle_password(nng_aio *aio)
{
    NEU_PROCESS_HTTP_REQUEST_VALIDATE_JWT(
        aio, neu_json_password_req_t, neu_json_decode_password_req, {
            neu_user_t *user     = NULL;
            int         rv       = 0;
            int         pass_len = strlen(req->new_pass);

            if (pass_len < NEU_USER_PASSWORD_MIN_LEN ||
                pass_len > NEU_USER_PASSWORD_MAX_LEN) {
                nlog_error("user `%s` new password too short or too long",
                           req->name);
                rv = NEU_ERR_INVALID_PASSWORD_LEN;
            } else if (NULL == (user = neu_load_user(req->name))) {
                nlog_error("could not find user `%s`", req->name);
                rv = NEU_ERR_INVALID_USER_OR_PASSWORD;
            } else if (!neu_user_check_password(user, req->old_pass)) {
                nlog_error("user `%s` password check fail", req->name);
                rv = NEU_ERR_INVALID_USER_OR_PASSWORD;
            } else if (0 !=
                       (rv = neu_user_update_password(user, req->new_pass))) {
                nlog_error("user `%s` update password fail", req->name);
            } else if (0 != (rv = neu_save_user(user))) {
                nlog_error("user `%s` persist fail", req->name);
            }

            NEU_JSON_RESPONSE_ERROR(rv, {
                neu_http_response(aio, error_code.error, result_error);
            });

            neu_user_free(user);
        })
}

static char *file_string_read(size_t *length, const char *const path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        errno   = 0;
        *length = 0;
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    *length = (size_t) ftell(fp);
    if (0 == *length) {
        fclose(fp);
        return NULL;
    }

    char *data = NULL;
    data       = (char *) malloc((*length + 1) * sizeof(char));
    if (NULL != data) {
        data[*length] = '\0';
        fseek(fp, 0, SEEK_SET);
        size_t read = fread(data, sizeof(char), *length, fp);
        if (read != *length) {
            *length = 0;
            free(data);
            data = NULL;
        }
    } else {
        *length = 0;
    }

    fclose(fp);
    return data;
}

static struct {
    const char *plugin;
    const char *schema;
} g_plugin_to_schema_map_[] = {
    {
        .plugin = "Beckhoff ADS",
        .schema = "ads",
    },
    {
        .plugin = "BACnet/IP",
        .schema = "bacnet",
    },
    {
        .plugin = "DLT645-2007",
        .schema = "dlt645",
    },
    {
        .plugin = "eKuiper",
        .schema = "ekuiper",
    },
    {
        .plugin = "EtherNet/IP(CIP)",
        .schema = "EtherNet-IP",
    },
    {
        .plugin = "Fanuc Focas Ethernet",
        .schema = "focas",
    },
    {
        .plugin = "File",
        .schema = "file",
    },
    {
        .plugin = "HJ212-2017",
        .schema = "hj",
    },
    {
        .plugin = "IEC60870-5-104",
        .schema = "iec104",
    },
    {
        .plugin = "IEC61850",
        .schema = "iec61850",
    },
    {
        .plugin = "KNXnet/IP",
        .schema = "knx",
    },
    {
        .plugin = "Mitsubishi 3E",
        .schema = "qna3e",
    },
    {
        .plugin = "Mitsubishi 1E",
        .schema = "a1e",
    },
    {
        .plugin = "Modbus RTU",
        .schema = "modbus-rtu",
    },
    {
        .plugin = "Modbus TCP",
        .schema = "modbus-tcp",
    },
    {
        .plugin = "Monitor",
        .schema = "monitor",
    },
    {
        .plugin = "MQTT",
        .schema = "mqtt",
    },
    {
        .plugin = "NON A11",
        .schema = "nona11",
    },
    {
        .plugin = "OPC UA",
        .schema = "opcua",
    },
    {
        .plugin = "Omron FINS",
        .schema = "fins",
    },
    {
        .plugin = "Profinet",
        .schema = "profinet",
    },
    {
        .plugin = "Siemens S7 ISOTCP",
        .schema = "s7comm",
    },
    {
        .plugin = "Siemens S7 ISOTCP for 300/400",
        .schema = "s7comm_for_300",
    },
    {
        .plugin = "Siemens FetchWrite",
        .schema = "s5fetch-write",
    },
    {
        .plugin = "SparkPlugB",
        .schema = "sparkplugb",
    },
    {
        .plugin = "WebSocket",
        .schema = "websocket",
    },
};

static inline const char *plugin_name_to_schema_name(const char *name)
{
    for (size_t i = 0; i <
         sizeof(g_plugin_to_schema_map_) / sizeof(g_plugin_to_schema_map_[0]);
         ++i) {
        if (0 == strcmp(name, g_plugin_to_schema_map_[i].plugin)) {
            return g_plugin_to_schema_map_[i].schema;
        }
    }

    return name;
}

void handle_get_plugin_schema(nng_aio *aio)
{
    char          param[NEU_PLUGIN_NAME_LEN] = { 0 };
    const char *  schema_name                = param;
    neu_plugin_t *plugin                     = neu_rest_get_plugin();

    NEU_VALIDATE_JWT(aio);

    int rv = neu_http_get_param_str(aio, "schema_name", param, sizeof(param));
    if (-2 == rv) {
        // fall back to `plugin_name` param
        rv = neu_http_get_param_str(aio, "plugin_name", param, sizeof(param));
        schema_name = plugin_name_to_schema_name(param);
    }
    if (rv < 0 || strlen(schema_name) >= NEU_PLUGIN_NAME_LEN) {
        neu_http_bad_request(aio, "{\"error\": 1002}");
        return;
    }

    int                    ret    = 0;
    neu_reqresp_head_t     header = { 0 };
    neu_req_check_schema_t cmd    = { 0 };

    header.otel_trace_type = NEU_OTEL_TRACE_TYPE_REST_COMM;

    header.ctx  = aio;
    header.type = NEU_REQ_CHECK_SCHEMA;
    strcpy(cmd.schema, schema_name);

    ret = neu_plugin_op(plugin, header, &cmd);
    if (ret != 0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_IS_BUSY, {
            neu_http_response(aio, NEU_ERR_IS_BUSY, result_error);
        });
    }
}

void handle_get_plugin_schema_resp(nng_aio *aio, neu_resp_check_schema_t *resp)
{
    if (resp->exist) {
        char * schema_path = NULL;
        size_t len         = 0;
        char * buf         = NULL;

        if (0 > neu_asprintf(&schema_path, "%s/schema/%s.json", g_plugin_dir,
                             resp->schema)) {
            NEU_JSON_RESPONSE_ERROR(NEU_ERR_EINTERNAL, {
                neu_http_response(aio, error_code.error, result_error);
            });
            return;
        }

        buf = file_string_read(&len, schema_path);
        if (NULL == buf) {
            free(schema_path);
            if (0 > neu_asprintf(&schema_path, "%s/custom/schema/%s.json",
                                 g_plugin_dir, resp->schema)) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_EINTERNAL, {
                    neu_http_response(aio, error_code.error, result_error);
                });
                return;
            }
            buf = file_string_read(&len, schema_path);
        }

        if (NULL == buf) {
            free(schema_path);
            if (0 > neu_asprintf(&schema_path, "%s/system/schema/%s.json",
                                 g_plugin_dir, resp->schema)) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_EINTERNAL, {
                    neu_http_response(aio, error_code.error, result_error);
                });
                return;
            }
            buf = file_string_read(&len, schema_path);
        }

        if (NULL == buf) {
            nlog_info("open %s error: %d", schema_path, errno);
            neu_http_not_found(aio, "{\"status\": \"error\"}");
            free(schema_path);
            return;
        }

        neu_http_ok(aio, buf);
        free(buf);
        free(schema_path);
    } else {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_PLUGIN_NOT_FOUND, {
            neu_http_response(aio, error_code.error, result_error);
        });
    }
}

void handle_status(nng_aio *aio)
{
    char *result = NULL;

    NEU_VALIDATE_JWT(aio);

    int ret = neu_asprintf(&result, "{\"status\":\"%s\"}", g_status);

    if (ret < 0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_EINTERNAL, {
            neu_http_response(aio, error_code.error, result_error);
        });
        return;
    }

    neu_http_response(aio, 0, result);
    free(result);
}

int nodes_write_to_csv(const char *filename, UT_array *nodes)
{
    FILE *f = fopen(filename, "a");
    if (NULL == f) {
        nlog_error("failed to open file:%s", filename);
        return -1;
    }

    utarray_foreach(nodes, neu_persist_node_info_t *, node)
    {
        fprintf(f, "%s,%d,%d,%s\n", node->name, node->type, node->state,
                node->plugin_name);
    }
    fclose(f);
    return 0;
}

int settings_write_to_csv(const char *filename, const char *node_name,
                          const char *settings)
{
    FILE *f = fopen(filename, "a");
    if (NULL == f) {
        nlog_error("failed to open file:%s", filename);
        return -1;
    }

    fprintf(f, "%s,%s\n", node_name, settings);
    fclose(f);
    return 0;
}

int groups_write_to_csv(const char *filename, const char *driver_name,
                        UT_array *groups)
{
    FILE *f = fopen(filename, "a");
    if (NULL == f) {
        nlog_error("failed to open file:%s", filename);
        return -1;
    }

    utarray_foreach(groups, neu_persist_group_info_t *, group)
    {
        fprintf(f, "%s,%s,%d, NULL\n", driver_name, group->name,
                group->interval);
    }
    fclose(f);
    return 0;
}

int subscribes_write_to_csv(const char *filename, const char *app_name,
                            UT_array *subscribers)
{
    FILE *f = fopen(filename, "a");
    if (NULL == f) {
        nlog_error("failed to open file:%s", filename);
        return -1;
    }

    utarray_foreach(subscribers, neu_persist_subscription_info_t *, sub)
    {
        fprintf(f, "%s,%s,%s,%s,%s\n", app_name, sub->driver_name,
                sub->group_name, sub->params, sub->static_tags);
    }
    fclose(f);
    return 0;
}

int tags_write_to_csv(const char *filename, const char *node_name,
                      const char *group_name, UT_array *tags)
{
    FILE *f = fopen(filename, "a");
    if (NULL == f) {
        nlog_error("failed to open file:%s", filename);
        return -1;
    }

    utarray_foreach(tags, neu_datatag_t *, tag)
    {
        fprintf(f, "%s,%s,%s,%s,%d,%d,%f,%f,%d,%s,%s,NULL\n", node_name,
                group_name, tag->name, tag->address, tag->attribute,
                tag->precision, tag->decimal, tag->bias, tag->type,
                tag->description, tag->format);
    }
    fclose(f);
    return 0;
}

int write_csv_header()
{
    FILE *f = fopen("nodes.csv", "w");
    if (NULL == f) {
        nlog_error("failed to open file:%s", "nodes.csv");
        return -1;
    }
    fprintf(f, "name,type,state,plugin_name\n");
    fclose(f);

    f = fopen("settings.csv", "w");
    if (NULL == f) {
        nlog_error("failed to open file:%s", "settings.csv");
        return -1;
    }
    fprintf(f, "node_name,setting\n");
    fclose(f);

    f = fopen("groups.csv", "w");
    if (NULL == f) {
        nlog_error("failed to open file:%s", "groups.csv");
        return -1;
    }
    fprintf(f, "driver_name,name,interval,context\n");
    fclose(f);

    f = fopen("tags.csv", "w");
    if (NULL == f) {
        nlog_error("failed to open file:%s", "tags.csv");
        return -1;
    }
    fprintf(f,
            "driver_name,group_name,name,address,attribute,precision,"
            "decimal,bias,type,description,format,value\n");
    fclose(f);

    f = fopen("subscribes.csv", "w");
    if (NULL == f) {
        nlog_error("failed to open file:%s", "subscribes.csv");
        return -1;
    }
    fprintf(f, "app_name,driver_name,group_name,params,static_tags\n");
    fclose(f);

    return 0;
}

int read_file(const char *file_name, void **datap, size_t *lenp)
{
    int         rv = 0;
    FILE *      f;
    struct stat st;
    size_t      len;
    void *      data;

    if (stat(file_name, &st) != 0) {
        return NEU_ERR_FILE_NOT_EXIST;
    }

    if ((f = fopen(file_name, "rb")) == NULL) {
        nlog_error("open fail: %s", file_name);
        return NEU_ERR_FILE_OPEN_FAILURE;
    }

    len = st.st_size;
    if (len > 0) {
        if ((data = malloc(len)) == NULL) {
            rv = NEU_ERR_EINTERNAL;
            goto done;
        }
        if (fread(data, 1, len, f) != len) {
            nlog_error("file read failued, errno = %d", errno);
            rv = NEU_ERR_FILE_READ_FAILURE;
            free(data);
            goto done;
        }
    } else {
        data = NULL;
    }

    *datap = data;
    *lenp  = len;
done:
    fclose(f);
    return (rv);
}

void handle_export_db(nng_aio *aio)
{
    write_csv_header();

    UT_array *nodes = NULL;
    int       rv    = neu_persister_load_nodes(&nodes);
    if (rv != 0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_FILE_WRITE_FAILURE, {
            neu_http_response(aio, error_code.error, result_error);
        });
        return;
    }

    rv = nodes_write_to_csv("nodes.csv", nodes);
    if (rv != 0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_FILE_WRITE_FAILURE, {
            neu_http_response(aio, error_code.error, result_error);
        });
        utarray_free(nodes);
        return;
    }

    utarray_foreach(nodes, neu_persist_node_info_t *, node)
    {
        char *settings = NULL;
        rv             = neu_persister_load_node_setting(node->name,
                                             (const char **) &settings);
        if (rv != 0) {
            continue;
        }
        rv = settings_write_to_csv("settings.csv", node->name, settings);
        free(settings);
        if (rv != 0) {
            NEU_JSON_RESPONSE_ERROR(NEU_ERR_FILE_WRITE_FAILURE, {
                neu_http_response(aio, error_code.error, result_error);
            });
            utarray_free(nodes);
            return;
        }
    }

    utarray_foreach(nodes, neu_persist_node_info_t *, node)
    {
        if (node->type == 2) {
            UT_array *subscribers = NULL;
            rv = neu_persister_load_subscriptions(node->name, &subscribers);
            if (rv != 0) {
                continue;
            }
            rv = subscribes_write_to_csv("subscribes.csv", node->name,
                                         subscribers);
            utarray_free(subscribers);
            if (rv != 0) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_FILE_WRITE_FAILURE, {
                    neu_http_response(aio, error_code.error, result_error);
                });
                utarray_free(nodes);
                return;
            }
        }
    }

    utarray_foreach(nodes, neu_persist_node_info_t *, node)
    {
        UT_array *groups = NULL;
        rv               = neu_persister_load_groups(node->name, &groups);
        if (rv != 0) {
            continue;
        }
        rv = groups_write_to_csv("groups.csv", node->name, groups);
        if (rv != 0) {
            NEU_JSON_RESPONSE_ERROR(NEU_ERR_FILE_WRITE_FAILURE, {
                neu_http_response(aio, error_code.error, result_error);
            });
            utarray_free(nodes);
            return;
        }

        utarray_foreach(groups, neu_persist_group_info_t *, group)
        {
            UT_array *tags = NULL;
            rv = neu_persister_load_tags(node->name, group->name, &tags);
            if (rv != 0) {
                continue;
            }
            rv = tags_write_to_csv("tags.csv", node->name, group->name, tags);
            if (rv != 0) {
                NEU_JSON_RESPONSE_ERROR(NEU_ERR_FILE_WRITE_FAILURE, {
                    neu_http_response(aio, error_code.error, result_error);
                });
                utarray_free(nodes);
                utarray_free(groups);
                return;
            }
            utarray_free(tags);
        }
        utarray_free(groups);
    }

    utarray_free(nodes);

    system("rm -rf /tmp/neuron.zip");
    system("zip /tmp/neuron.zip nodes.csv settings.csv groups.csv tags.csv "
           "subscribes.csv");

    void * data = NULL;
    size_t len  = 0;

    rv = read_file("/tmp/neuron.zip", &data, &len);
    nlog_info("read_file /tmp/neuron.zip rv=%d, len=%zu", rv, len);
    if (rv != 0) {
        NEU_JSON_RESPONSE_ERROR(NEU_ERR_FILE_READ_FAILURE, {
            neu_http_response(aio, error_code.error, result_error);
        });
        return;
    }
    neu_http_response_file(aio, data, len,
                           "attachment; filename=\"neuron.zip\"");
    free(data);
}
