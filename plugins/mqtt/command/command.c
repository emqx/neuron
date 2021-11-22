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

    neu_json_mqtt_t *mqtt    = NULL;
    char *           ret_str = NULL;
    int              rc      = neu_json_decode_mqtt_req(json_str, &mqtt);

    if (0 != rc) {
        log_error("JSON parsing mqtt failed");
        return;
    }

    switch (mqtt->function) {
    case NEU_MQTT_OP_GET_GROUP_CONFIG: {
        neu_json_get_group_config_req_t *req = NULL;
        rc = neu_json_decode_get_group_config_req(json_str, &req);
        if (0 == rc) {
            ret_str = command_get_group_configs(plugin, mqtt, req);
            neu_json_decode_get_group_config_req_free(req);
        }
        break;
    }
    case NEU_MQTT_OP_ADD_GROUP_CONFIG: {
        neu_json_add_group_config_req_t *req = NULL;
        rc = neu_json_decode_add_group_config_req(json_str, &req);
        if (0 == rc) {
            ret_str = command_add_group_config(plugin, mqtt, req);
            neu_json_decode_add_group_config_req_free(req);
        }
        break;
    }
    case NEU_MQTT_OP_UPDATE_GROUP_CONFIG: {
        neu_json_update_group_config_req_t *req = NULL;
        rc = neu_json_decode_update_group_config_req(json_str, &req);
        if (0 == rc) {
            ret_str = command_update_group_config(plugin, mqtt, req);
            neu_json_decode_update_group_config_req_free(req);
        }
        break;
    }
    case NEU_MQTT_OP_DELETE_GROUP_CONFIG: {
        neu_json_del_group_config_req_t *req = NULL;
        rc = neu_json_decode_del_group_config_req(json_str, &req);
        if (0 == rc) {
            ret_str = command_delete_group_config(plugin, mqtt, req);
            neu_json_decode_del_group_config_req_free(req);
        }
        break;
    }
    case NEU_MQTT_OP_READ: {
        neu_json_read_req_t *req = NULL;
        rc                       = neu_json_decode_read_req(json_str, &req);
        if (0 == rc) {
            uint32_t req_id = neu_plugin_get_event_id(plugin);
            if (0 < req_id) {
                response->context_add(plugin, req_id, mqtt, NULL, false);
            }

            command_read_once_request(plugin, mqtt, req, req_id);
            neu_json_decode_read_req_free(req);
        }
        break;
    }
    case NEU_MQTT_OP_WRITE: {
        neu_json_write_req_t *req = NULL;
        rc                        = neu_json_decode_write_req(json_str, &req);
        if (0 == rc) {
            uint32_t req_id = neu_plugin_get_event_id(plugin);
            if (0 < req_id) {
                response->context_add(plugin, req_id, mqtt, NULL, false);
            }

            command_write_request(plugin, mqtt, req, req_id);
            neu_json_decode_write_req_free(req);
        }
        break;
    }
    case NEU_MQTT_OP_GET_TAGS: {
        neu_json_get_tags_req_t *req = NULL;
        rc = neu_json_decode_get_tags_req(json_str, &req);
        if (0 == rc) {
            ret_str = command_get_tags(plugin, mqtt, req);
            neu_json_decode_get_tags_req_free(req);
        }
        break;
    }
    case NEU_MQTT_OP_ADD_TAGS: {
        neu_json_add_tags_req_t *req = NULL;
        rc = neu_json_decode_add_tags_req(json_str, &req);
        if (0 == rc) {
            ret_str = command_add_tags(plugin, mqtt, req);
            neu_json_decode_add_tags_req_free(req);
        }
        break;
    }
    case NEU_MQTT_OP_UPDATE_TAGS: {
        neu_json_update_tags_req_t *req = NULL;
        rc = neu_json_decode_update_tags_req(json_str, &req);
        if (0 == rc) {
            ret_str = command_update_tags(plugin, mqtt, req);
            neu_json_decode_update_tags_req_free(req);
        }
        break;
    }
    case NEU_MQTT_OP_DELETE_TAGS: {
        neu_json_del_tags_req_t *req = NULL;
        rc = neu_json_decode_del_tags_req(json_str, &req);
        if (0 == rc) {
            ret_str = command_delete_tags(plugin, mqtt, req);
            neu_json_decode_del_tags_req_free(req);
        }
        break;
    }
    case NEU_MQTT_OP_GET_NODES: {
        neu_json_get_nodes_req_t *req = NULL;
        rc = neu_json_decode_get_nodes_req(json_str, &req);
        if (0 == rc) {
            ret_str = command_nodes_get(plugin, mqtt, req);
            neu_json_decode_get_nodes_req_free(req);
        }
        break;
    }
    case NEU_MQTT_OP_ADD_NODES: {
        neu_json_add_node_req_t *req = NULL;
        rc = neu_json_decode_add_node_req(json_str, &req);
        if (0 == rc) {
            ret_str = command_node_add(plugin, mqtt, req);
            neu_json_decode_add_node_req_free(req);
        }
        break;
    }
    case NEU_MQTT_OP_UPDATE_NODES: {
        neu_json_update_node_req_t *req = NULL;
        rc = neu_json_decode_update_node_req(json_str, &req);
        if (0 == rc) {
            ret_str = command_node_update(plugin, mqtt, req);
            neu_json_decode_update_node_req_free(req);
        }
        break;
    }
    case NEU_MQTT_OP_DELETE_NODES: {
        neu_json_del_node_req_t *req = NULL;
        rc = neu_json_decode_del_node_req(json_str, &req);
        if (0 == rc) {
            ret_str = command_node_delete(plugin, mqtt, req);
            neu_json_decode_del_node_req_free(req);
        }
        break;
    }
    case NEU_MQTT_OP_NODE_SETTING: {
        neu_json_node_setting_req_t *req = NULL;
        rc = neu_json_decode_node_setting_req(json_str, &req);
        if (0 == rc) {
            ret_str = command_node_setting_set(plugin, mqtt, req, json_str);
            neu_json_decode_node_setting_req_free(req);
        }
        break;
    }
    case NEU_MQTT_OP_GET_NODE_SETTING: {
        neu_json_node_setting_req_t *req = NULL;
        rc = neu_json_decode_node_setting_req(json_str, &req);
        if (0 == rc) {
            ret_str = command_node_setting_get(plugin, mqtt, req);
            neu_json_decode_node_setting_req_free(req);
        }
        break;
    }
    case NEU_MQTT_OP_GET_NODE_STATE: {
        neu_json_node_setting_req_t *req = NULL;
        rc = neu_json_decode_node_setting_req(json_str, &req);
        if (0 == rc) {
            ret_str = command_node_state_get(plugin, mqtt, req);
            neu_json_decode_node_setting_req_free(req);
        }
        break;
    }
    case NEU_MQTT_OP_NODE_CTL: {
        neu_json_node_ctl_req_t *req = NULL;
        rc = neu_json_decode_node_ctl_req(json_str, &req);
        if (0 == rc) {
            ret_str = command_node_control(plugin, mqtt, req);
            neu_json_decode_node_ctl_req_free(req);
        }
        break;
    }
    default:
        break;
    }

    if (NULL != ret_str) {
        response->context_add(plugin, 0, mqtt, ret_str, true);
    }

    if (NULL != json_str) {
        free(json_str);
    }
}
