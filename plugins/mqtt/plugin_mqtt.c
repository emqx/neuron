#include <assert.h>
#include <pthread.h>
#include <stdlib.h>

#include <neuron.h>

#include "parser/neu_json_parser.h"
#include "parser/neu_json_read.h"
#include "parser/neu_json_write.h"

#include "option.h"
#include "paho_client.h"

#define UNUSED(x) (void) (x)

const neu_plugin_module_t neu_plugin_module;

struct neu_plugin {
    neu_plugin_common_t common;
    option_t            option;
    paho_client_t *     paho;
    uint32_t            new_event_id;
    neu_list            context_list;
};

typedef struct {
    neu_list_node   node;
    char            uuid[36 + 1];
    int             function;
    int *           cmd_req_id;
    int             n_cmd_req;
    neu_variable_t *variables;
    int             n_completed;
} context_t;

static uint32_t plugin_get_event_id(neu_plugin_t *plugin)
{
    uint32_t req_id;

    plugin->new_event_id++;
    if (plugin->new_event_id == 0) {
        plugin->new_event_id = 1;
    }

    req_id = plugin->new_event_id;
    return req_id;
}

static uint32_t plugin_get_nodes(neu_plugin_t *plugin)
{
    int                        rv;
    const adapter_callbacks_t *adapter_callbacks;
    adapter_callbacks = plugin->common.adapter_callbacks;

    /* example of get nodes */
    neu_request_t        sync_cmd;
    neu_cmd_get_nodes_t  get_nodes_cmd;
    neu_response_t *     result = NULL;
    neu_reqresp_nodes_t *resp_nodes;

    get_nodes_cmd.node_type = NEU_NODE_TYPE_DRIVER;
    sync_cmd.req_type       = NEU_REQRESP_GET_NODES;
    sync_cmd.req_id         = plugin_get_event_id(plugin);
    sync_cmd.buf            = (void *) &get_nodes_cmd;
    sync_cmd.buf_len        = sizeof(neu_cmd_get_nodes_t);
    log_info("Get list of driver nodes");
    rv = adapter_callbacks->command(plugin->common.adapter, &sync_cmd, &result);
    if (rv < 0) {
        return 0;
    }

    neu_node_info_t *node_info;
    neu_node_id_t    dst_node_id;
    assert(result->buf_len == sizeof(neu_reqresp_nodes_t));
    resp_nodes = (neu_reqresp_nodes_t *) result->buf;
    node_info  = (neu_node_info_t *) vector_get(&resp_nodes->nodes, 0);

    // neu_node_info_t *n;
    // VECTOR_FOR_EACH(&resp_nodes->nodes, iter)
    // {
    //     n = (neu_node_info_t *) iterator_get(&iter);
    //     log_info("node info:%s-%u", n->node_name, n->node_id);
    // }

    dst_node_id = node_info->node_id;
    log_info("The first node(%d) of driver nodes list", dst_node_id);
    vector_uninit(&resp_nodes->nodes);
    free(resp_nodes);
    free(result);

    return dst_node_id;
}

static neu_taggrp_config_t *plugin_get_group(neu_plugin_t *plugin,
                                             uint32_t      dest_node_id)
{
    int                        rv;
    const adapter_callbacks_t *adapter_callbacks;
    adapter_callbacks = plugin->common.adapter_callbacks;

    /* example of get group configs */
    neu_request_t   sync_cmd;
    neu_response_t *result = NULL;

    neu_cmd_get_grp_configs_t  get_grps_cmd;
    neu_reqresp_grp_configs_t *resp_grps;
    result = NULL;

    get_grps_cmd.node_id = dest_node_id;
    sync_cmd.req_type    = NEU_REQRESP_GET_GRP_CONFIGS;
    sync_cmd.req_id      = plugin_get_event_id(plugin);
    sync_cmd.buf         = (void *) &get_grps_cmd;
    sync_cmd.buf_len     = sizeof(neu_cmd_get_grp_configs_t);
    log_info("Get list of group configs");
    rv = adapter_callbacks->command(plugin->common.adapter, &sync_cmd, &result);
    if (rv < 0) {
        return NULL;
    }

    neu_taggrp_config_t *grp_config;
    assert(result->buf_len == sizeof(neu_reqresp_grp_configs_t));
    resp_grps = (neu_reqresp_grp_configs_t *) result->buf;
    grp_config =
        *(neu_taggrp_config_t **) vector_get(&resp_grps->grp_configs, 1);

    // neu_taggrp_config_t *c;
    // VECTOR_FOR_EACH(&resp_grps->grp_configs, iter)
    // {
    //     c = *(neu_taggrp_config_t **) iterator_get(&iter);
    //     log_info("config name:%s", neu_taggrp_cfg_get_name(c));
    // }

    neu_taggrp_cfg_ref(grp_config);

    log_info("The first group config(%s) of node(%d)",
             neu_taggrp_cfg_get_name(grp_config), dest_node_id);
    VECTOR_FOR_EACH(&resp_grps->grp_configs, iter)
    {
        neu_taggrp_config_t *cur_grp_config;
        cur_grp_config = *(neu_taggrp_config_t **) iterator_get(&iter);
        neu_taggrp_cfg_free(cur_grp_config);
    }
    vector_uninit(&resp_grps->grp_configs);
    free(resp_grps);
    free(result);

    return grp_config;
}

static int plugin_adapter_read_command(neu_plugin_t *plugin, const char *group)
{
    uint32_t             dest_node_id = plugin_get_nodes(plugin);
    neu_taggrp_config_t *group_config = plugin_get_group(plugin, dest_node_id);

    const adapter_callbacks_t *adapter_callbacks;
    adapter_callbacks = plugin->common.adapter_callbacks;

    neu_request_t      cmd;
    neu_reqresp_read_t read_req;

    read_req.grp_config =
        (neu_taggrp_config_t *) neu_taggrp_cfg_ref(group_config);
    read_req.dst_node_id = dest_node_id;

    cmd.req_type = NEU_REQRESP_READ_DATA;
    cmd.req_id   = plugin_get_event_id(plugin);
    cmd.buf      = (void *) &read_req;
    cmd.buf_len  = sizeof(neu_reqresp_read_t);
    log_info("Send a read command, dst_node_id:%d, req id:%d",
             read_req.dst_node_id, cmd.req_id);
    adapter_callbacks->command(plugin->common.adapter, &cmd, NULL);
    UNUSED(group);
    return cmd.req_id;
}

static int plugin_adapter_write_command(neu_plugin_t *       plugin,
                                        enum neu_json_type   type,
                                        union neu_json_value value)
{
    const adapter_callbacks_t *adapter_callbacks;
    adapter_callbacks = plugin->common.adapter_callbacks;

    neu_request_t       cmd1;
    neu_reqresp_write_t write_req;
    neu_variable_t *    data_var = neu_variable_create();

    if (NEU_JSON_INT == type) {
        neu_variable_set_qword(data_var, value.val_int);
    }

    if (NEU_JSON_STR == type) {
        neu_variable_set_string(data_var, value.val_str);
    }
    if (NEU_JSON_DOUBLE == type) {
        neu_variable_set_double(data_var, value.val_double);
    }

    write_req.grp_config  = NULL;
    write_req.dst_node_id = 1;
    write_req.addr        = 3;
    write_req.data_var    = data_var;
    cmd1.req_type         = NEU_REQRESP_WRITE_DATA;
    cmd1.req_id           = plugin_get_event_id(plugin);
    cmd1.buf              = (void *) &write_req;
    cmd1.buf_len          = sizeof(neu_reqresp_write_t);
    log_info("Send a write command");
    adapter_callbacks->command(plugin->common.adapter, &cmd1, NULL);
    neu_variable_destroy(data_var);
    return cmd1.req_id;
}

static int plugin_subscribe(neu_plugin_t *plugin, const char *topic,
                            const int qos, subscribe_handle handle)
{
    client_error error =
        paho_client_subscribe(plugin->paho, topic, qos, handle);
    if (ClientSubscribeFailure == error ||
        ClientSubscribeAddListFailure == error) {
        neu_panic("Subscribe Failure");
    }
    return 0;
}

static int plugin_send(neu_plugin_t *plugin, char *buff, const size_t len)
{
    client_error error = paho_client_publish(
        plugin->paho, "neuronlite/response", 0, (unsigned char *) buff, len);
    return error;
}

static void plugin_read_handle(neu_plugin_t *             plugin,
                               struct neu_parse_read_req *req)
{
    log_info("READ uuid:%s, group:%s", req->uuid, req->group);

    if (0 >= req->n_name) {
        return;
    }

    context_t *context = (context_t *) malloc(sizeof(context_t));
    if (NULL == context) {
        return;
    }
    memset(context, 0, sizeof(context_t));

    memcpy(context->uuid, req->uuid, 36);
    context->function = req->function;

    context->cmd_req_id = (int *) malloc(sizeof(int) * req->n_name);
    if (NULL == context->cmd_req_id) {
        free(context);
        return;
    }

    context->n_cmd_req = req->n_name;
    context->variables =
        (neu_variable_t *) malloc(sizeof(neu_variable_t) * req->n_name);
    if (NULL == context->variables) {
        free(context->cmd_req_id);
        free(context);
        return;
    }

    plugin_adapter_read_command(plugin, req->group);
}

static void plugin_write_handle(neu_plugin_t *              plugin,
                                struct neu_parse_write_req *write_req)
{
    log_info("WRITE uuid:%s, group:%s", write_req->uuid, write_req->group);

    for (int i = 0; i < write_req->n_tag; i++) {
        plugin_adapter_write_command(plugin, write_req->tags[i].t,
                                     write_req->tags[i].value);
    }

    // Response status
    char *                     result = NULL;
    struct neu_parse_write_res res    = {
        .function = NEU_PARSE_OP_WRITE,
        .uuid     = write_req->uuid,
        .error    = 0,
    };

    int rc = neu_parse_encode(&res, &result);
    if (0 == rc) {
        plugin_send(plugin, result, strlen(result));
        free(result);
    }
}

static void plugin_response_handle(const char *topic_name, size_t topic_len,
                                   void *payload, const size_t len,
                                   void *context)
{
    if (NULL == topic_name || NULL == payload) {
        return;
    }

    char *json_str = malloc(len + 1);
    if (NULL == json_str) {
        return;
    }

    memset(json_str, 0x00, len + 1);
    memcpy(json_str, payload, len);

    neu_plugin_t *             plugin = (neu_plugin_t *) context;
    void *                     result = NULL;
    struct neu_parse_read_req *req    = NULL;
    int                        rc     = neu_parse_decode(json_str, &result);
    req                               = (struct neu_parse_read_req *) result;
    if (0 != rc) {
        log_error("JSON parsing failed");
        return;
    }

    if (NEU_PARSE_OP_READ == req->function) {
        plugin_read_handle(plugin, req);
    }

    if (NEU_PARSE_OP_WRITE == req->function) {
        struct neu_parse_write_req *write_req;
        write_req = (struct neu_parse_write_req *) result;
        plugin_write_handle(plugin, write_req);
    }

    neu_parse_decode_free(result);
    free(json_str);

    UNUSED(topic_len);
}

static neu_plugin_t *mqtt_plugin_open(neu_adapter_t *            adapter,
                                      const adapter_callbacks_t *callbacks)
{
    if (NULL == adapter || NULL == callbacks) {
        return NULL;
    }

    neu_plugin_t *plugin;
    plugin = (neu_plugin_t *) malloc(sizeof(neu_plugin_t));
    if (NULL == plugin) {
        log_error("Failed to allocate plugin %s",
                  neu_plugin_module.module_name);
        return NULL;
    }

    plugin->common.adapter           = adapter;
    plugin->common.adapter_callbacks = callbacks;
    log_info("Success to create plugin: %s", neu_plugin_module.module_name);
    return plugin;
}

static int mqtt_plugin_close(neu_plugin_t *plugin)
{
    free(plugin);
    log_info("Success to free plugin:%s", neu_plugin_module.module_name);
    return 0;
}

static int mqtt_plugin_init(neu_plugin_t *plugin)
{
    plugin->new_event_id = 1;

    // MQTT option
    plugin->option.clientid           = "neuron-lite-mqtt-plugin";
    plugin->option.MQTT_version       = 4;
    plugin->option.topic              = "neuronlite/response";
    plugin->option.qos                = 1;
    plugin->option.connection         = "tcp://";
    plugin->option.host               = "broker.emqx.io";
    plugin->option.port               = "1883";
    plugin->option.keepalive_interval = 20;
    plugin->option.clean_session      = 1;

    // Paho mqtt client setup
    client_error error =
        paho_client_open(&plugin->option, plugin, &plugin->paho);
    if (ClientIsNULL == error) {
        return -1;
    }

    plugin_subscribe(plugin, "neuronlite/request", 0, plugin_response_handle);
    return 0;
}

static int mqtt_plugin_uninit(neu_plugin_t *plugin)
{
    paho_client_close(plugin->paho);

    return 0;
}

static int mqtt_plugin_config(neu_plugin_t *plugin, neu_config_t *configs)
{
    UNUSED(plugin);
    UNUSED(configs);
    return 0;
}

static int mqtt_plugin_request(neu_plugin_t *plugin, neu_request_t *req)
{
    int rv = 0;

    if (plugin == NULL || req == NULL) {
        log_warn("The plugin pointer or request is NULL");
        return (-1);
    }

    log_info("send request to plugin: %s", neu_plugin_module.module_name);

    switch (req->req_type) {
    case NEU_REQRESP_TRANS_DATA: {
        neu_reqresp_data_t *neu_data;
        assert(req->buf_len == sizeof(neu_reqresp_data_t));
        neu_data                 = (neu_reqresp_data_t *) req->buf;
        neu_variable_t *variable = neu_data->data_var;
        size_t          count    = neu_variable_count(variable);
        log_debug("variable count: %ld", count);

        break;
    }

    case NEU_REQRESP_WRITE_DATA: {
        break;
    }

    default:
        break;
    }
    return rv;
}

static int mqtt_plugin_event_reply(neu_plugin_t *     plugin,
                                   neu_event_reply_t *reply)
{
    UNUSED(plugin);
    UNUSED(reply);
    return 0;
}

static const neu_plugin_intf_funs_t plugin_intf_funs = {
    .open        = mqtt_plugin_open,
    .close       = mqtt_plugin_close,
    .init        = mqtt_plugin_init,
    .uninit      = mqtt_plugin_uninit,
    .config      = mqtt_plugin_config,
    .request     = mqtt_plugin_request,
    .event_reply = mqtt_plugin_event_reply
};

const neu_plugin_module_t neu_plugin_module = {
    .version      = NEURON_PLUGIN_VER_1_0,
    .module_name  = "neuron-mqtt-plugin",
    .module_descr = "Neuron northbound MQTT communication plugin",
    .intf_funs    = &plugin_intf_funs
};
