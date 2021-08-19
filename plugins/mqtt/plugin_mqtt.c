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
    neu_list            send_list;
    neu_list            arrived_list;
    pthread_mutex_t     send_mutex;
    pthread_mutex_t     arrived_mutex;
    pthread_t           send_thread_id;
    pthread_t           arrived_thread_id;
    int                 finished;
    uint32_t            new_event_id;
};

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

typedef struct {
    neu_list_node node;
    char *        topic;
    char *        json_str;
    int           error;
} arrived_task_t;

typedef struct {
    neu_list_node      node;
    unsigned char *    buffer;
    size_t             buffer_size;
    int                task_id;
    neu_reqresp_type_e task_type;
} send_task_t;

static void plugin_send_task_destroy(send_task_t *task)
{
    if (NULL == task) {
        return;
    }

    if (NULL != task->buffer) {
        free(task->buffer);
    }

    free(task);
}

static void plugin_arrived_task_destroy(arrived_task_t *task)
{
    if (NULL == task) {
        return;
    }

    if (NULL != task->json_str) {
        free(task->json_str);
    }

    if (NULL != task->topic) {
        free(task->topic);
    }

    free(task);
}

static int plugin_add_arrived_task(neu_plugin_t *plugin, arrived_task_t *task)
{
    pthread_mutex_lock(&plugin->arrived_mutex);
    NEU_LIST_NODE_INIT(&task->node);
    neu_list_append(&plugin->arrived_list, task);
    pthread_mutex_unlock(&plugin->arrived_mutex);
    return 0;
}

// static int plugin_add_send_task(neu_plugin_t *plugin, send_task_t *task)
// {
//     pthread_mutex_lock(&plugin->send_mutex);
//     NEU_LIST_NODE_INIT(&task->node);
//     neu_list_append(&plugin->send_list, task);
//     pthread_mutex_unlock(&plugin->send_mutex);
//     return 0;
// }

static int plugin_adapter_response(neu_plugin_t *plugin, send_task_t *task)
{
    const adapter_callbacks_t *adapter_callbacks;
    adapter_callbacks = plugin->common.adapter_callbacks;

    neu_response_t     resp;
    neu_reqresp_data_t neu_data;
    neu_variable_t     data_var;
    static const char *resp_str = "MQTT plugin read response";

    data_var.var_type.typeId = NEU_DATATYPE_STRING;
    data_var.data            = (void *) resp_str;

    neu_data.grp_config = NULL;
    neu_data.data_var   = &data_var;

    memset(&resp, 0, sizeof(resp));
    resp.req_id    = task->task_id;
    resp.resp_type = NEU_REQRESP_TRANS_DATA;
    resp.buf_len   = sizeof(neu_reqresp_data_t);
    resp.buf       = &neu_data;

    return adapter_callbacks->response(plugin->common.adapter, &resp);
}

static int plugin_adapter_read_command(neu_plugin_t *plugin, const char *tag)
{
    const adapter_callbacks_t *adapter_callbacks;
    adapter_callbacks = plugin->common.adapter_callbacks;

    neu_request_t      cmd;
    neu_reqresp_read_t read_req;

    read_req.grp_config  = NULL;
    read_req.dst_node_id = 1;

    if (0 == strcmp("tag001", tag)) {
        read_req.addr = 5;
    }

    if (0 == strcmp("tag002", tag)) {
        read_req.addr = 4;
    }

    if (0 == strcmp("tag003", tag)) {
        read_req.addr = 3;
    }

    cmd.req_type = NEU_REQRESP_READ_DATA;
    cmd.req_id   = plugin_get_event_id(plugin);
    cmd.buf      = (void *) &read_req;
    cmd.buf_len  = sizeof(neu_reqresp_read_t);
    log_info("Send a read command");

    adapter_callbacks->command(plugin->common.adapter, &cmd, NULL);
    return 0;
}

static int plugin_adapter_write_command(neu_plugin_t *       plugin,
                                        enum neu_json_type   type,
                                        union neu_json_value value)
{
    const adapter_callbacks_t *adapter_callbacks;
    adapter_callbacks = plugin->common.adapter_callbacks;

    neu_request_t       cmd1;
    neu_reqresp_write_t write_req;
    neu_variable_t      data_var;
    // static const char * data_str = "Sample app writing";

    if (NEU_JSON_INT == type) {
        data_var.var_type.typeId = NEU_DATATYPE_QWORD;
        data_var.data            = (void *) &value.val_int;
    }

    if (NEU_JSON_STR == type) {
        data_var.var_type.typeId = NEU_DATATYPE_STRING;
        data_var.data            = (void *) value.val_str;
    }
    if (NEU_JSON_DOUBLE == type) {
        data_var.var_type.typeId = NEU_DATATYPE_DOUBLE;
        data_var.data            = (void *) &value.val_double;
    }

    write_req.grp_config  = NULL;
    write_req.dst_node_id = 1;
    write_req.addr        = 3;
    write_req.data_var    = &data_var;
    cmd1.req_type         = NEU_REQRESP_WRITE_DATA;
    cmd1.req_id           = plugin_get_event_id(plugin);
    cmd1.buf              = (void *) &write_req;
    cmd1.buf_len          = sizeof(neu_reqresp_write_t);
    log_info("Send a write command");
    adapter_callbacks->command(plugin->common.adapter, &cmd1, NULL);
    return 0;
}

static int plugin_send(neu_plugin_t *plugin, char *buff, const size_t len)
{
    client_error error = paho_client_publish(
        plugin->paho, "neuronlite/response", 0, (unsigned char *) buff, len);
    return error;
}

static void plugin_send_loop(neu_plugin_t *plugin)
{
    while (0 == plugin->finished) {
        usleep(10 * 1000);
        pthread_mutex_lock(&plugin->send_mutex);

        if (neu_list_empty(&plugin->send_list)) {
            pthread_mutex_unlock(&plugin->send_mutex);
            continue;
        }

        send_task_t *task = neu_list_last(&plugin->send_list);
        if (NULL == task) {
            pthread_mutex_unlock(&plugin->send_mutex);
            continue;
        }

        plugin_adapter_response(plugin, task);
        neu_list_remove(&plugin->send_list, task);
        plugin_send_task_destroy(task);
        pthread_mutex_unlock(&plugin->send_mutex);
    }
}

static void plugin_arrived_loop(neu_plugin_t *plugin)
{
    while (0 == plugin->finished) {
        usleep(10 * 1000);
        pthread_mutex_lock(&plugin->arrived_mutex);

        if (neu_list_empty(&plugin->arrived_list)) {
            pthread_mutex_unlock(&plugin->arrived_mutex);
            continue;
        }

        arrived_task_t *task = neu_list_last(&plugin->arrived_list);
        if (NULL == task) {
            pthread_mutex_unlock(&plugin->arrived_mutex);
            continue;
        }

        if (NULL == task->json_str) {
            pthread_mutex_unlock(&plugin->arrived_mutex);
            continue;
        }

        // JSON deserailize
        void *                     result = NULL;
        struct neu_parse_read_req *req    = NULL;
        int rc = neu_parse_decode(task->json_str, &result);
        if (0 != rc) {
            task->error = 1;
        }
        req = (struct neu_parse_read_req *) result;

        if (NEU_PARSE_OP_READ == req->function) {
            log_info("READ uuid:%s, group:%s", req->uuid, req->group);
            for (int i = 0; i < req->n_name; i++) {
                plugin_adapter_read_command(plugin, req->names[i].name);
            }
        }

        if (NEU_PARSE_OP_WRITE == req->function) {
            log_info("WRITE uuid:%s, group:%s", req->uuid, req->group);
            struct neu_parse_write_req *write_req;
            write_req = (struct neu_parse_write_req *) result;
            for (int i = 0; i < write_req->n_tag; i++) {
                plugin_adapter_write_command(plugin, write_req->tags[i].t,
                                             write_req->tags[i].value);
            }

            // Response status
            char *                     result = NULL;
            struct neu_parse_write_res res    = {
                .function = NEU_PARSE_OP_WRITE,
                .uuid     = req->uuid,
                .error    = 0,
            };

            rc = neu_parse_encode(&res, &result);
            if (0 == rc) {
                plugin_send(plugin, result, strlen(result));
                free(result);
            }
        }

        neu_parse_decode_free(result);
        neu_list_remove(&plugin->arrived_list, task);
        plugin_arrived_task_destroy(task);
        pthread_mutex_unlock(&plugin->arrived_mutex);
    }
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

static void plugin_response_handle(const char *topic_name, size_t topic_len,
                                   void *payload, const size_t len,
                                   void *context)
{
    if (NULL == topic_name || NULL == payload) {
        return;
    }

    neu_plugin_t *  plugin = (neu_plugin_t *) context;
    arrived_task_t *task   = (arrived_task_t *) malloc(sizeof(arrived_task_t));
    memset(task, 0x00, sizeof(arrived_task_t));

    task->topic    = strdup(topic_name);
    task->json_str = malloc(len + 1);
    memset(task->json_str, 0x00, len + 1);
    memcpy(task->json_str, payload, len);

    UNUSED(topic_len);

    plugin_add_arrived_task(plugin, task);
    return;
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

    // Work list init
    NEU_LIST_INIT(&plugin->arrived_list, arrived_task_t, node);
    NEU_LIST_INIT(&plugin->send_list, send_task_t, node);

    // Work thread create
    plugin->finished = 0;
    pthread_mutex_init(&plugin->send_mutex, NULL);
    pthread_mutex_init(&plugin->arrived_mutex, NULL);

    int rc = pthread_create(&plugin->send_thread_id, NULL,
                            (void *) plugin_send_loop, plugin);
    if (0 != rc) {
        log_error("Create publish thread faild.");
        return -1;
    }
    pthread_detach(plugin->send_thread_id);

    rc = pthread_create(&plugin->arrived_thread_id, NULL,
                        (void *) plugin_arrived_loop, plugin);
    if (0 != rc) {
        log_error("Create arrived thread faild.");
        return -1;
    }
    pthread_detach(plugin->arrived_thread_id);

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
    plugin->finished = 1;
    paho_client_close(plugin->paho);

    // Work list cleanup
    while (!neu_list_empty(&plugin->arrived_list)) {
        arrived_task_t *task = neu_list_first(&plugin->arrived_list);
        neu_list_remove(&plugin->arrived_list, task);
        plugin_arrived_task_destroy(task);
    }
    while (neu_list_empty(&plugin->send_list)) {
        send_task_t *task = neu_list_first(&plugin->send_list);
        neu_list_remove(&plugin->send_list, task);
        plugin_send_task_destroy(task);
    }
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
    // const adapter_callbacks_t *adapter_callbacks;
    // adapter_callbacks = plugin->common.adapter_callbacks;

    switch (req->req_type) {
    case NEU_REQRESP_TRANS_DATA: {
        neu_reqresp_data_t *neu_data;
        // const char *        req_str;

        assert(req->buf_len == sizeof(neu_reqresp_data_t));
        neu_data = (neu_reqresp_data_t *) req->buf;

        char *                    result = NULL;
        struct neu_parse_read_res res    = {
            .function = NEU_PARSE_OP_READ,
            .uuid     = (char *) "554f5fd8-f437-11eb-975c-7704b9e17821",
            .error    = 0,
            .n_tag    = 1,
        };

        res.tags = (struct neu_parse_read_res_tag *) calloc(
            1, sizeof(struct neu_parse_read_res_tag));

        int type = neu_data->data_var->var_type.typeId;
        if (type == NEU_DATATYPE_STRING) {
            const char *data_str;
            data_str              = neu_variable_get_str(neu_data->data_var);
            res.tags[0].name      = strdup((char *) "tag003");
            res.tags[0].type      = NEU_JSON_STR;
            res.tags[0].timestamp = 0;
            res.tags[0].value     = 0;
            int rc                = neu_parse_encode(&res, &result);
            if (0 == rc) {
                plugin_send(plugin, result, strlen(result));
            }

            free(res.tags[0].name);
            free(res.tags);
            free(result);

            log_info("MQTT plug-in read NEU_DATATYPE_STRING value: %s",
                     data_str);
        }
        if (type == NEU_DATATYPE_DOUBLE) {
            double data_d         = *(double *) neu_data->data_var->data;
            res.tags[0].name      = strdup((char *) "tag002");
            res.tags[0].type      = NEU_JSON_DOUBLE;
            res.tags[0].timestamp = 0;
            res.tags[0].value     = data_d;
            int rc                = neu_parse_encode(&res, &result);
            if (0 == rc) {
                plugin_send(plugin, result, strlen(result));
            }

            free(res.tags[0].name);
            free(res.tags);
            free(result);

            log_info("MQTT plug-in NEU_DATATYPE_DOUBLE value: %lf", data_d);
        }
        if (type == NEU_DATATYPE_QWORD) {
            int64_t data_i64      = *(int64_t *) neu_data->data_var->data;
            res.tags[0].name      = strdup((char *) "tag001");
            res.tags[0].type      = NEU_JSON_INT;
            res.tags[0].timestamp = 0;
            res.tags[0].value     = data_i64;
            int rc                = neu_parse_encode(&res, &result);
            if (0 == rc) {
                plugin_send(plugin, result, strlen(result));
            }
            free(res.tags[0].name);
            free(res.tags);
            free(result);

            log_info("MQTT plug-in read NEU_DATATYPE_QWORD value: %ld",
                     data_i64);
        }
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
