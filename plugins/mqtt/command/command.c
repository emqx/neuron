#include <stdio.h>
#include <stdlib.h>

#include "command.h"

void command_response_handle(mqtt_response_t *response)
{
    neu_plugin_t *plugin = (neu_plugin_t *) response->context;

    if (NULL == response->topic_name || NULL == response->payload) {
        return;
    }

    char *json_str = malloc(response->len + 1);
    if (NULL == json_str) {
        return;
    }

    memset(json_str, 0x00, response->len + 1);
    memcpy(json_str, response->payload, response->len);

    neu_parse_mqtt_t *mqtt    = NULL;
    char *            ret_str = NULL;
    int               rc      = neu_parse_decode_mqtt_param(json_str, &mqtt);

    if (0 != rc) {
        log_error("JSON parsing mqtt failed");
        return;
    }

    switch (mqtt->function) {
    case NEU_MQTT_OP_GET_GROUP_CONFIG: {
        neu_parse_get_group_config_req_t *req = NULL;
        rc = neu_parse_decode_get_group_config(json_str, &req);
        if (0 == rc) {
            ret_str = command_get_group_configs(plugin, mqtt, req);
            neu_parse_decode_get_group_config_free(req);
        }
        break;
    }
    case NEU_MQTT_OP_ADD_GROUP_CONFIG: {
        neu_parse_add_group_config_req_t *req = NULL;
        rc = neu_parse_decode_add_group_config(json_str, &req);
        if (0 == rc) {
            ret_str = command_add_group_config(plugin, mqtt, req);
            neu_parse_decode_add_group_config_free(req);
        }
        break;
    }
    case NEU_MQTT_OP_UPDATE_GROUP_CONFIG: {
        neu_parse_update_group_config_req_t *req = NULL;
        rc = neu_parse_decode_update_group_config(json_str, &req);
        if (0 == rc) {
            ret_str = command_update_group_config(plugin, mqtt, req);
            neu_parse_decode_update_group_config_free(req);
        }
        break;
    }
    case NEU_MQTT_OP_DELETE_GROUP_CONFIG: {
        neu_parse_del_group_config_req_t *req = NULL;
        rc = neu_parse_decode_del_group_config(json_str, &req);
        if (0 == rc) {
            ret_str = command_delete_group_config(plugin, mqtt, req);
            neu_parse_decode_del_group_config_free(req);
        }
        break;
    }
    case NEU_MQTT_OP_READ: {
        neu_parse_read_req_t *req = NULL;
        rc                        = neu_parse_decode_read(json_str, &req);
        if (0 == rc) {
            uint32_t req_id = neu_plugin_get_event_id(plugin);
            if (0 < req_id) {
                response->context_add(plugin, req_id, mqtt);
            }

            command_read_once_request(plugin, mqtt, req, req_id);
            neu_parse_decode_read_free(req);
        }
        break;
    }
    case NEU_MQTT_OP_WRITE: {
        neu_parse_write_req_t *req = NULL;
        rc                         = neu_parse_decode_write(json_str, &req);
        if (0 == rc) {
            uint32_t req_id = neu_plugin_get_event_id(plugin);
            if (0 < req_id) {
                response->context_add(plugin, req_id, mqtt);
            }

            command_write_request(plugin, mqtt, req, req_id);
            neu_parse_decode_write_free(req);
        }
        break;
    }
    case NEU_MQTT_OP_GET_TAGS: {
        neu_parse_get_tags_req_t *req = NULL;
        rc = neu_parse_decode_get_tags_req(json_str, &req);
        if (0 == rc) {
            ret_str = command_get_tags(plugin, mqtt, req);
            neu_parse_decode_get_tags_req_free(req);
        }
        break;
    }
    case NEU_MQTT_OP_ADD_TAGS: {
        neu_parse_add_tags_req_t *req = NULL;
        rc = neu_parse_decode_add_tags_req(json_str, &req);
        if (0 == rc) {
            ret_str = command_add_tags(plugin, mqtt, req);
            neu_parse_decode_add_tags_req_free(req);
        }
        break;
    }
    case NEU_MQTT_OP_UPDATE_TAGS: {
        neu_parse_update_tags_req_t *req = NULL;
        rc = neu_parse_decode_update_tags_req(json_str, &req);
        if (0 == rc) {
            ret_str = command_update_tags(plugin, mqtt, req);
            neu_parse_decode_update_tags_req_free(req);
        }
        break;
    }
    case NEU_MQTT_OP_DELETE_TAGS: {
        neu_parse_del_tags_req_t *req = NULL;
        rc = neu_parse_decode_del_tags_req(json_str, &req);
        if (0 == rc) {
            ret_str = command_delete_tags(plugin, mqtt, req);
            neu_parse_decode_del_tags_req_free(req);
        }
        break;
    }
    case NEU_MQTT_OP_GET_NODES: {
        neu_parse_get_nodes_req_t *req = NULL;
        rc = neu_parse_decode_get_nodes(json_str, &req);
        if (0 == rc) {
            ret_str = command_get_nodes(plugin, mqtt, req);
            neu_parse_decode_get_nodes_free(req);
        }
        break;
    }
    case NEU_MQTT_OP_ADD_NODES: {
        neu_parse_add_node_req_t *req = NULL;
        rc = neu_parse_decode_add_node(json_str, &req);
        if (0 == rc) {
            ret_str = command_add_node(plugin, mqtt, req);
            neu_parse_decode_add_node_free(req);
        }
        break;
    }
    case NEU_MQTT_OP_UPDATE_NODES: {
        neu_parse_update_node_req_t *req = NULL;
        rc = neu_parse_decode_update_node(json_str, &req);
        if (0 == rc) {
            ret_str = command_update_node(plugin, mqtt, req);
            neu_parse_decode_update_node_free(req);
        }
        break;
    }
    case NEU_MQTT_OP_DELETE_NODES: {
        neu_parse_del_node_req_t *req = NULL;
        rc = neu_parse_decode_del_node(json_str, &req);
        if (0 == rc) {
            ret_str = command_delete_node(plugin, mqtt, req);
            neu_parse_decode_del_node_free(req);
        }
        break;
    }
    default:
        break;
    }

    response->mqtt_send(plugin, ret_str);

    if (NULL != mqtt) {
        if (NULL != mqtt->uuid) {
            free(mqtt->uuid);
        }
        free(mqtt);
    }

    if (NULL != json_str) {
        free(json_str);
    }
}
