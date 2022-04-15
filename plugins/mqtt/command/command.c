#include <stdio.h>
#include <stdlib.h>

#include "datatag.h"
#include "group_config.h"
#include "node.h"
#include "ping.h"
#include "plugin.h"
#include "read_write.h"
#include "schema.h"
#include "tty.h"

#include "command.h"

// Ping topic
static int ping_response(char **output_str, neu_plugin_t *plugin,
                         neu_json_mqtt_t *mqtt)
{
    *output_str = command_ping(plugin, mqtt);
    return 0;
}

// Maniplate node
static int node_response_get(char **output_str, neu_plugin_t *plugin,
                             neu_json_mqtt_t *mqtt, char *json_str)
{
    neu_json_get_nodes_req_t *req = NULL;
    int rc = neu_json_decode_get_nodes_req(json_str, &req);
    if (0 == rc) {
        *output_str = command_node_get(plugin, mqtt, req);
        neu_json_decode_get_nodes_req_free(req);
    }

    return rc;
}

static int node_response_add(char **output_str, neu_plugin_t *plugin,
                             neu_json_mqtt_t *mqtt, char *json_str)
{
    neu_json_add_node_req_t *req = NULL;
    int                      rc  = neu_json_decode_add_node_req(json_str, &req);
    if (0 == rc) {
        *output_str = command_node_add(plugin, mqtt, req);
        neu_json_decode_add_node_req_free(req);
    }

    return rc;
}

static int node_response_delete(char **output_str, neu_plugin_t *plugin,
                                neu_json_mqtt_t *mqtt, char *json_str)
{
    neu_json_del_node_req_t *req = NULL;
    int                      rc  = neu_json_decode_del_node_req(json_str, &req);
    if (0 == rc) {
        *output_str = command_node_delete(plugin, mqtt, req);
        neu_json_decode_del_node_req_free(req);
    }

    return rc;
}

// Manipulate group config
static int group_config_response_get(char **output_str, neu_plugin_t *plugin,
                                     neu_json_mqtt_t *mqtt, char *json_str)
{
    neu_json_get_group_config_req_t *req = NULL;
    int rc = neu_json_decode_get_group_config_req(json_str, &req);
    if (0 == rc) {
        *output_str = command_group_config_get(plugin, mqtt, req);
        neu_json_decode_get_group_config_req_free(req);
    }

    return rc;
}

static int group_config_response_add(char **output_str, neu_plugin_t *plugin,
                                     neu_json_mqtt_t *mqtt, char *json_str)
{
    neu_json_add_group_config_req_t *req = NULL;
    int rc = neu_json_decode_add_group_config_req(json_str, &req);
    if (0 == rc) {
        *output_str = command_group_config_add(plugin, mqtt, req);
        neu_json_decode_add_group_config_req_free(req);
    }

    return rc;
}

static int group_config_response_update(char **output_str, neu_plugin_t *plugin,
                                        neu_json_mqtt_t *mqtt, char *json_str)
{
    neu_json_update_group_config_req_t *req = NULL;
    int rc = neu_json_decode_update_group_config_req(json_str, &req);
    if (0 == rc) {
        *output_str = command_group_config_update(plugin, mqtt, req);
        neu_json_decode_update_group_config_req_free(req);
    }

    return rc;
}

static int group_config_response_delete(char **output_str, neu_plugin_t *plugin,
                                        neu_json_mqtt_t *mqtt, char *json_str)
{
    neu_json_del_group_config_req_t *req = NULL;
    int rc = neu_json_decode_del_group_config_req(json_str, &req);
    if (0 == rc) {
        *output_str = command_group_config_delete(plugin, mqtt, req);
        neu_json_decode_del_group_config_req_free(req);
    }

    return rc;
}

// Manipulate data tags
static int datatag_response_get(char **output_str, neu_plugin_t *plugin,
                                neu_json_mqtt_t *mqtt, char *json_str)
{
    neu_json_get_tags_req_t *req = NULL;
    int                      rc  = neu_json_decode_get_tags_req(json_str, &req);
    *output_str                  = command_get_tags(plugin, mqtt, req);
    if (0 == rc) {
        neu_json_decode_get_tags_req_free(req);
    }

    return rc;
}

static int datatag_response_add(char **output_str, neu_plugin_t *plugin,
                                neu_json_mqtt_t *mqtt, char *json_str)
{
    neu_json_add_tags_req_t *req = NULL;
    int                      rc  = neu_json_decode_add_tags_req(json_str, &req);
    *output_str                  = command_add_tags(plugin, mqtt, req);
    if (0 == rc) {
        neu_json_decode_add_tags_req_free(req);
    }

    return rc;
}

static int datatag_response_update(char **output_str, neu_plugin_t *plugin,
                                   neu_json_mqtt_t *mqtt, char *json_str)
{
    neu_json_update_tags_req_t *req = NULL;
    int rc      = neu_json_decode_update_tags_req(json_str, &req);
    *output_str = command_update_tags(plugin, mqtt, req);
    if (0 == rc) {
        neu_json_decode_update_tags_req_free(req);
    }

    return rc;
}

static int datatag_response_delete(char **output_str, neu_plugin_t *plugin,
                                   neu_json_mqtt_t *mqtt, char *json_str)
{
    neu_json_del_tags_req_t *req = NULL;
    int                      rc  = neu_json_decode_del_tags_req(json_str, &req);
    *output_str                  = command_delete_tags(plugin, mqtt, req);
    if (0 == rc) {
        neu_json_decode_del_tags_req_free(req);
    }

    return rc;
}

// Manipulate plugin libs
static int plugin_response_get(char **output_str, neu_plugin_t *plugin,
                               neu_json_mqtt_t *mqtt, char *json_str)
{
    neu_json_get_plugin_req_t *req = NULL;
    int rc = neu_json_decode_get_plugin_req(json_str, &req);
    if (0 == rc) {
        *output_str = command_plugin_get(plugin, mqtt, req);
        neu_json_decode_get_plugin_req_free(req);
    }

    return rc;
}

static int plugin_response_add(char **output_str, neu_plugin_t *plugin,
                               neu_json_mqtt_t *mqtt, char *json_str)
{
    neu_json_add_plugin_req_t *req = NULL;
    int rc = neu_json_decode_add_plugin_req(json_str, &req);
    if (0 == rc) {
        *output_str = command_plugin_add(plugin, mqtt, req);
        neu_json_decode_add_plugin_req_free(req);
    }

    return rc;
}

// static int plugin_response_update(char **output_str, neu_plugin_t *plugin,
//                                   neu_json_mqtt_t *mqtt, char *json_str)
// {
//     return 0;
// }

static int plugin_response_delete(char **output_str, neu_plugin_t *plugin,
                                  neu_json_mqtt_t *mqtt, char *json_str)
{
    neu_json_del_plugin_req_t *req = NULL;
    int rc = neu_json_decode_del_plugin_req(json_str, &req);
    if (0 == rc) {
        *output_str = command_plugin_delete(plugin, mqtt, req);
        neu_json_decode_del_plugin_req_free(req);
    }

    return rc;
}

// Subscribe group config
static int subscribe_response_get(char **output_str, neu_plugin_t *plugin,
                                  neu_json_mqtt_t *mqtt, char *json_str)
{
    neu_json_get_group_config_req_t *req = NULL;
    int rc = neu_json_decode_get_group_config_req(json_str, &req);
    if (0 == rc) {
        *output_str = command_get_group_config_subscribes(plugin, mqtt, req);
        neu_json_decode_get_group_config_req_free(req);
    }

    return rc;
}

static int subscribe_response_add(char **output_str, neu_plugin_t *plugin,
                                  neu_json_mqtt_t *mqtt, char *json_str)
{
    neu_json_subscribe_req_t *req = NULL;
    int rc = neu_json_decode_subscribe_req(json_str, &req);
    if (0 == rc) {
        *output_str = command_group_config_subscribe(plugin, mqtt, req);
        neu_json_decode_subscribe_req_free(req);
    }

    return rc;
}

static int subscribe_response_delete(char **output_str, neu_plugin_t *plugin,
                                     neu_json_mqtt_t *mqtt, char *json_str)
{
    neu_json_unsubscribe_req_t *req = NULL;
    int rc = neu_json_decode_unsubscribe_req(json_str, &req);
    if (0 == rc) {
        *output_str = command_group_config_unsubscribe(plugin, mqtt, req);
        neu_json_decode_unsubscribe_req_free(req);
    }

    return rc;
}

// Read tags with group config
static int read_response(mqtt_response_t *response, neu_plugin_t *plugin,
                         neu_json_mqtt_t *mqtt, char *json_str)
{
    neu_json_read_req_t *req = NULL;
    int                  rc  = neu_json_decode_read_req(json_str, &req);
    if (0 == rc) {
        uint32_t req_id = neu_plugin_get_event_id(plugin);
        if (0 < req_id) {
            response->context_add(plugin, req_id, mqtt, NULL,
                                  response->topic_pair, false);
        }

        command_rw_read_once_request(plugin, mqtt, req, req_id);
        neu_json_decode_read_req_free(req);
    }

    return rc;
}

// Write tags with group config
static int write_response(mqtt_response_t *response, neu_plugin_t *plugin,
                          neu_json_mqtt_t *mqtt, char *json_str)
{
    neu_json_write_req_t *req = NULL;
    int                   rc  = neu_json_decode_write_req(json_str, &req);
    if (0 == rc) {
        uint32_t req_id = neu_plugin_get_event_id(plugin);
        int      ret    = command_rw_write_request(plugin, mqtt, req, req_id);

        if (0 < req_id && ret == 0) {
            response->context_add(plugin, req_id, mqtt, NULL,
                                  response->topic_pair, false);
        }

        neu_json_decode_write_req_free(req);
    }

    return rc;
}

// Manipulate TTY
static int tty_response_get(char **output_str, neu_plugin_t *plugin,
                            neu_json_mqtt_t *mqtt, char *json_str)
{
    UNUSED(json_str);
    *output_str = command_tty_get(plugin, mqtt);
    return 0;
}

// Manipulate plugin schema
static int schema_response_get(char **output_str, neu_plugin_t *plugin,
                               neu_json_mqtt_t *mqtt, char *json_str)
{
    UNUSED(json_str);
    *output_str = command_schema_get(plugin, mqtt);
    return 0;
}

// Manipulate node setting
static int setting_response_set(char **output_str, neu_plugin_t *plugin,
                                neu_json_mqtt_t *mqtt, char *json_str)
{
    neu_json_node_setting_req_t *req = NULL;
    int rc = neu_json_decode_node_setting_req(json_str, &req);
    if (0 == rc) {
        *output_str = command_node_setting_set(plugin, mqtt, req, json_str);
        neu_json_decode_node_setting_req_free(req);
    }

    return rc;
}

static int setting_response_get(char **output_str, neu_plugin_t *plugin,
                                neu_json_mqtt_t *mqtt, char *json_str)
{
    neu_json_node_setting_req_t *req = NULL;
    int rc = neu_json_decode_node_setting_req(json_str, &req);
    if (0 == rc) {
        *output_str = command_node_setting_get(plugin, mqtt, req);
        neu_json_decode_node_setting_req_free(req);
    }

    return rc;
}

// Control the node
static int control_response(char **output_str, neu_plugin_t *plugin,
                            neu_json_mqtt_t *mqtt, char *json_str)
{
    neu_json_node_ctl_req_t *req = NULL;
    int                      rc  = neu_json_decode_node_ctl_req(json_str, &req);
    if (0 == rc) {
        *output_str = command_node_control(plugin, mqtt, req);
        neu_json_decode_node_ctl_req_free(req);
    }

    return rc;
}

// Manipulate node state
static int state_response(char **output_str, neu_plugin_t *plugin,
                          neu_json_mqtt_t *mqtt, char *json_str)
{
    neu_json_node_setting_req_t *req = NULL;
    int rc = neu_json_decode_node_setting_req(json_str, &req);
    if (0 == rc) {
        *output_str = command_node_state_get(plugin, mqtt, req);
        neu_json_decode_node_setting_req_free(req);
    }

    return rc;
}

void command_response_handle(mqtt_response_t *response)
{
    neu_plugin_t *plugin = (neu_plugin_t *) response->plugin;

    if (NULL == response->topic_name || NULL == response->payload) {
        return;
    }

    char *json_str = malloc(response->len + 1);
    if (NULL == json_str) {
        return;
    }

    memset(json_str, 0x00, response->len + 1);
    memcpy(json_str, response->payload, response->len);

    neu_json_mqtt_t *mqtt    = NULL;
    char *           ret_str = NULL;
    int              rc      = neu_json_decode_mqtt_req(json_str, &mqtt);

    if (0 != rc) {
        log_error("JSON parsing mqtt failed");
        return;
    }

    if (0 != strcmp(mqtt->command, NEU_MQTT_CMD_GET) &&
        0 != strcmp(mqtt->command, NEU_MQTT_CMD_SET) &&
        0 != strcmp(mqtt->command, NEU_MQTT_CMD_ADD) &&
        0 != strcmp(mqtt->command, NEU_MQTT_CMD_UPDATE) &&
        0 != strcmp(mqtt->command, NEU_MQTT_CMD_DELETE) &&
        0 != strcmp(mqtt->command, NEU_MQTT_CMD_NONE)) {
        log_error("Unsupported command");
        return;
    }

    switch (response->type) {
    case TOPIC_TYPE_PING: {
        // Ping
        rc = ping_response(&ret_str, plugin, mqtt);
        break;
    }

    case TOPIC_TYPE_NODE: {
        // Node GET/ADD/UPDATE/DELETE
        if (0 == strcmp(NEU_MQTT_CMD_GET, mqtt->command)) {
            rc = node_response_get(&ret_str, plugin, mqtt, json_str);
        } else if (0 == strcmp(NEU_MQTT_CMD_ADD, mqtt->command)) {
            rc = node_response_add(&ret_str, plugin, mqtt, json_str);
        } else if (0 == strcmp(NEU_MQTT_CMD_DELETE, mqtt->command)) {
            rc = node_response_delete(&ret_str, plugin, mqtt, json_str);
        }

        break;
    }

    case TOPIC_TYPE_GCONFIG: {
        // Group Config GET/ADD/UPDATE/DELETE
        if (0 == strcmp(NEU_MQTT_CMD_GET, mqtt->command)) {
            rc = group_config_response_get(&ret_str, plugin, mqtt, json_str);
        } else if (0 == strcmp(NEU_MQTT_CMD_ADD, mqtt->command)) {
            rc = group_config_response_add(&ret_str, plugin, mqtt, json_str);
        } else if (0 == strcmp(NEU_MQTT_CMD_UPDATE, mqtt->command)) {
            rc = group_config_response_update(&ret_str, plugin, mqtt, json_str);
        } else if (0 == strcmp(NEU_MQTT_CMD_DELETE, mqtt->command)) {
            rc = group_config_response_delete(&ret_str, plugin, mqtt, json_str);
        }

        break;
    }

    case TOPIC_TYPE_TAGS: {
        // Tag GET/ADD/UPDATE/DELETE
        if (0 == strcmp(NEU_MQTT_CMD_GET, mqtt->command)) {
            rc = datatag_response_get(&ret_str, plugin, mqtt, json_str);
        } else if (0 == strcmp(NEU_MQTT_CMD_ADD, mqtt->command)) {
            rc = datatag_response_add(&ret_str, plugin, mqtt, json_str);
        } else if (0 == strcmp(NEU_MQTT_CMD_UPDATE, mqtt->command)) {
            rc = datatag_response_update(&ret_str, plugin, mqtt, json_str);
        } else if (0 == strcmp(NEU_MQTT_CMD_DELETE, mqtt->command)) {
            rc = datatag_response_delete(&ret_str, plugin, mqtt, json_str);
        }

        break;
    }

    case TOPIC_TYPE_PLUGIN: {
        // Plugin GET/ADD/UPDATE/DELETE
        if (0 == strcmp(NEU_MQTT_CMD_GET, mqtt->command)) {
            rc = plugin_response_get(&ret_str, plugin, mqtt, json_str);
        } else if (0 == strcmp(NEU_MQTT_CMD_ADD, mqtt->command)) {
            rc = plugin_response_add(&ret_str, plugin, mqtt, json_str);
        } else if (0 == strcmp(NEU_MQTT_CMD_DELETE, mqtt->command)) {
            rc = plugin_response_delete(&ret_str, plugin, mqtt, json_str);
        }

        break;
    }

    case TOPIC_TYPE_SUBSCRIBE: {
        // Subscribe GET/ADD/DELETE
        if (0 == strcmp(NEU_MQTT_CMD_ADD, mqtt->command)) {
            rc = subscribe_response_add(&ret_str, plugin, mqtt, json_str);
        } else if (0 == strcmp(NEU_MQTT_CMD_DELETE, mqtt->command)) {
            rc = subscribe_response_delete(&ret_str, plugin, mqtt, json_str);
        } else if (0 == strcmp(NEU_MQTT_CMD_GET, mqtt->command)) {
            rc = subscribe_response_get(&ret_str, plugin, mqtt, json_str);
        }

        break;
    }

    case TOPIC_TYPE_READ: {
        rc = read_response(response, plugin, mqtt, json_str);
        break;
    }

    case TOPIC_TYPE_WRITE: {
        rc = write_response(response, plugin, mqtt, json_str);
        break;
    }

    case TOPIC_TYPE_TTYS: {
        // TTY GET
        if (0 == strcmp(NEU_MQTT_CMD_GET, mqtt->command)) {
            rc = tty_response_get(&ret_str, plugin, mqtt, json_str);
        }
        break;
    }

    case TOPIC_TYPE_SCHEMA: {
        // Schema GET
        if (0 == strcmp(NEU_MQTT_CMD_GET, mqtt->command)) {
            rc = schema_response_get(&ret_str, plugin, mqtt, json_str);
        }
        break;
    }

    case TOPIC_TYPE_SETTING: {
        // Setting SET/GET
        if (0 == strcmp(NEU_MQTT_CMD_SET, mqtt->command)) {
            rc = setting_response_set(&ret_str, plugin, mqtt, json_str);
        } else if (0 == strcmp(NEU_MQTT_CMD_GET, mqtt->command)) {
            rc = setting_response_get(&ret_str, plugin, mqtt, json_str);
        }

        break;
    }

    case TOPIC_TYPE_CTR: {
        rc = control_response(&ret_str, plugin, mqtt, json_str);
        break;
    }

    case TOPIC_TYPE_STATE: {
        rc = state_response(&ret_str, plugin, mqtt, json_str);
        break;
    }
    default:
        assert(1 == 0);
        break;
    }

    if (0 != rc) {
        neu_json_error_resp_t error = { .error = NEU_ERR_BODY_IS_WRONG };
        neu_json_encode_with_mqtt(&error, neu_json_encode_error_resp, mqtt,
                                  neu_json_encode_mqtt_resp, &ret_str);
    }

    if (NULL != ret_str) {
        response->context_add(plugin, 0, mqtt, ret_str, response->topic_pair,
                              true);
    }

    if (NULL != json_str) {
        free(json_str);
    }

    neu_json_decode_mqtt_req_free(mqtt);
}

char *command_read_once_response(neu_plugin_t *plugin, uint32_t node_id,
                                 neu_json_mqtt_t *parse_header,
                                 neu_data_val_t * resp_val)
{
    return command_rw_read_once_response(plugin, node_id, parse_header,
                                         resp_val);
}

char *command_read_periodic_response(neu_plugin_t *plugin, uint64_t sender,
                                     const char *         node_name,
                                     neu_taggrp_config_t *config,
                                     neu_data_val_t *     resp_val,
                                     int                  upload_format)
{
    return command_rw_read_periodic_response(plugin, sender, node_name, config,
                                             resp_val, upload_format);
}

char *command_write_response(neu_json_mqtt_t *parse_header,
                             neu_data_val_t * resp_val)
{
    return command_rw_write_response(parse_header, resp_val);
}