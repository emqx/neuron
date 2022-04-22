#include <stdio.h>
#include <stdlib.h>

#include "ping.h"
#include "read_write.h"

#include "command.h"

// Ping topic
static int ping_response(char **output_str, neu_plugin_t *plugin,
                         neu_json_mqtt_t *mqtt)
{
    *output_str = command_ping(plugin, mqtt);
    return 0;
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

    switch (response->type) {
    case TOPIC_TYPE_PING: {
        // Ping
        rc = ping_response(&ret_str, plugin, mqtt);
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