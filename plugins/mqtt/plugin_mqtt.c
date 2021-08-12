#include <pthread.h>
#include <stdlib.h>

#include <neuron.h>

#include "option.h"
#include "paho_client.h"

#define UNUSED(x) (void) (x)

const neu_plugin_module_t neu_plugin_module;

struct neu_plugin {
    neu_plugin_common_t common;
    option_t            option;
    paho_client_t *     paho;
    vector_t            send_vector;
    vector_t            arrived_vector;
    pthread_mutex_t     send_mutex;
    pthread_mutex_t     arrived_mutex;
    pthread_t           send_thread_id;
    pthread_t           arrived_thread_id;
    int                 finished;
};

typedef struct {
} arrived_task_t;

typedef struct {
    unsigned char *    buffer;
    size_t             buffer_size;
    int                task_id;
    neu_reqresp_type_e task_type;
} send_task_t;

static void plugin_send_task_destroy(send_task_t *task)
{
    if (NULL != task) {
        if (NULL != task->buffer) {
            free(task->buffer);
        }

        free(task);
    }
}

static void plugin_arrived_task_destroy(arrived_task_t *task)
{
    if (NULL != task) {
        free(task);
    }
}

static int plugin_add_arrived_task(neu_plugin_t *plugin, arrived_task_t *task)
{
    pthread_mutex_lock(&plugin->arrived_mutex);
    int rc = vector_push_back(&plugin->arrived_vector, task);
    if (0 != rc) {
        pthread_mutex_unlock(&plugin->arrived_mutex);
        log_error("Push back task failed");
        return -1;
    }
    pthread_mutex_unlock(&plugin->arrived_mutex);
    return 0;
}

static int plugin_add_send_task(neu_plugin_t *plugin, send_task_t *send_task)
{
    pthread_mutex_lock(&plugin->send_mutex);
    int rc = vector_push_back(&plugin->send_vector, send_task);
    if (0 != rc) {
        pthread_mutex_unlock(&plugin->send_mutex);
        log_error("Push back task failed");
        return -1;
    }
    pthread_mutex_unlock(&plugin->send_mutex);
    return 0;
}

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

static int plugin_adapter_response_error(neu_plugin_t *plugin,
                                         send_task_t * task)
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

static int plugin_adapter_event_notify(neu_plugin_t *  plugin,
                                       arrived_task_t *task)
{
    UNUSED(plugin);
    UNUSED(task);

    const adapter_callbacks_t *adapter_callbacks;
    adapter_callbacks = plugin->common.adapter_callbacks;

    neu_event_notify_t *event =
        (neu_event_notify_t *) malloc(sizeof(neu_event_notify_t));
    adapter_callbacks->event_notify(plugin->common.adapter, event);

    return 0;
}

static void plugin_send_loop(neu_plugin_t *plugin)
{
    while (0 == plugin->finished) {
        usleep(10 * 1000);
        pthread_mutex_lock(&plugin->send_mutex);

        if (vector_is_empty(&plugin->send_vector)) {
            pthread_mutex_unlock(&plugin->send_mutex);
            continue;
        }

        send_task_t *task = (send_task_t *) vector_front(&plugin->send_vector);
        if (NULL == task) {
            pthread_mutex_unlock(&plugin->send_mutex);
            continue;
        }

        // Process task
        client_error error = paho_client_publish(
            plugin->paho, "MQTT Examples", 0, task->buffer, task->buffer_size);
        if (ClientSuccess != error) {
            plugin_adapter_response_error(plugin, task);
        }

        plugin_adapter_response(plugin, task);

        vector_pop_front(&plugin->send_vector);
        plugin_send_task_destroy(task);

        pthread_mutex_unlock(&plugin->send_mutex);
    }
}

static void plugin_arrived_loop(neu_plugin_t *plugin)
{
    while (0 == plugin->finished) {
        usleep(10 * 1000);
        pthread_mutex_lock(&plugin->arrived_mutex);

        if (vector_is_empty(&plugin->arrived_vector)) {
            pthread_mutex_unlock(&plugin->arrived_mutex);
            continue;
        }

        arrived_task_t *task =
            (arrived_task_t *) vector_front(&plugin->arrived_vector);
        if (NULL == task) {
            pthread_mutex_unlock(&plugin->arrived_mutex);
            continue;
        }

        plugin_adapter_event_notify(plugin, task);

        vector_pop_front(&plugin->arrived_vector);
        plugin_arrived_task_destroy(task);

        pthread_mutex_unlock(&plugin->arrived_mutex);
    }
}

static int plugin_subscribe(neu_plugin_t *plugin, const char *topic,
                            const int qos, subscribe_handle handle)
{
    client_error error =
        paho_client_subscribe(plugin->paho, topic, qos, handle);
    if (0 > error) {
        // Do nothing
    }
    if (ClientSubscribeFailure == error ||
        ClientSubscribeAddListFailure == error) {
        // TODO: error handle -Panic?
    }
    return 0;
}

static void plugin_mqtt_examples_handle(const char *topic_name,
                                        size_t topic_len, void *payload,
                                        const size_t len, void *context)
{
    char payload_string[128] = { '\0' };
    memcpy(payload_string, payload, len);
    log_info("topic name:%s, topic len:%ld, payload:%s, payload "
             "len:%ld, paho "
             "plugin address:%x",
             topic_name, topic_len, payload_string, len, context);
    return;
}

static void plugin_response_handle(const char *topic_name, size_t topic_len,
                                   void *payload, const size_t len,
                                   void *context)
{
    neu_plugin_t *  plugin = (neu_plugin_t *) context;
    arrived_task_t *task   = (arrived_task_t *) malloc(sizeof(arrived_task_t));

    // TODO: generate task
    UNUSED(topic_name);
    UNUSED(topic_len);
    UNUSED(payload);
    UNUSED(len);

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
    // MQTT option
    plugin->option.clientid           = "neuron-lite-mqtt-plugin";
    plugin->option.MQTT_version       = 4;
    plugin->option.topic              = "MQTT Examples";
    plugin->option.qos                = 1;
    plugin->option.connection         = "tcp://";
    plugin->option.host               = "broker.emqx.io";
    plugin->option.port               = "1883";
    plugin->option.keepalive_interval = 20;
    plugin->option.clean_session      = 1;

    // Work list init
    vector_setup(&plugin->send_vector, 128, sizeof(send_task_t));
    if (!vector_is_initialized(&plugin->send_vector)) {
        log_error("Failed to initialize send vector");
        return -1;
    }

    vector_setup(&plugin->arrived_vector, 128, sizeof(arrived_task_t));
    if (!vector_is_initialized(&plugin->arrived_vector)) {
        log_error("Failed to initialize arrived vector");
        return -1;
    }

    // Publish thread create
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

    // Work thread create
    rc = pthread_create(&plugin->arrived_thread_id, NULL,
                        (void *) plugin_arrived_loop, plugin);
    if (0 != rc) {
        log_error("Create arrived thread faild.");
        return -1;
    }
    pthread_detach(plugin->arrived_thread_id);

    // paho mqtt client setup
    client_error error =
        paho_client_open(&plugin->option, plugin, &plugin->paho);
    if (ClientIsNULL == error) {
        return -1;
    }

    plugin_subscribe(plugin, "MQTT Examples", 0, plugin_mqtt_examples_handle);
    plugin_subscribe(plugin, "neuronlite/response", 0, plugin_response_handle);

    return 0;
}

static int mqtt_plugin_uninit(neu_plugin_t *plugin)
{
    plugin->finished = 1;
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
    UNUSED(plugin);
    UNUSED(req);

    int rv = 0;

    if (plugin == NULL || req == NULL) {
        log_warn("The plugin pointer or request is NULL");
        return (-1);
    }

    log_info("send request to plugin: %s", neu_plugin_module.module_name);

    switch (req->req_type) {
    case NEU_REQRESP_READ_DATA: {
        send_task_t *task = (send_task_t *) malloc(sizeof(send_task_t));
        task->task_id     = req->req_id;
        task->buffer      = (unsigned char *) malloc(req->buf_len);
        task->task_type   = NEU_REQRESP_READ_DATA;
        memcpy(task->buffer, req->buf, req->buf_len);
        plugin_add_send_task(plugin, task);
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
