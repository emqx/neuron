#include <pthread.h>
#include <stdlib.h>

#include <neuron.h>

#include "option.h"
#include "paho_client.h"

#define UNUSED(x) (void) (x)

static option_t           option;
const neu_plugin_module_t neu_plugin_module;

struct neu_plugin {
    neu_plugin_common_t common;
    paho_client_t *     paho;
    vector_t            publish_list;
    vector_t            arrived_list;
    pthread_mutex_t     publish_mutex;
    pthread_mutex_t     work_mutex;
    pthread_t           publish_thread_id;
    pthread_t           work_thread_id;
    int                 finished;
};

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

static void publish_loop(neu_plugin_t *plugin)
{
    while (0 == plugin->finished) {
        pthread_mutex_lock(&plugin->publish_mutex);
        sleep(1);
        pthread_mutex_unlock(&plugin->publish_mutex);
    }
}

static void work_loop(neu_plugin_t *plugin)
{
    while (0 == plugin->finished) {
        pthread_mutex_lock(&plugin->work_mutex);
        sleep(1);
        pthread_mutex_unlock(&plugin->work_mutex);
    }
}

static int mqtt_plugin_init(neu_plugin_t *plugin)
{
    option.clientid           = "neuron-lite-mqtt-plugin";
    option.MQTT_version       = 4;
    option.topic              = "MQTT Examples";
    option.qos                = 1;
    option.connection         = "tcp://";
    option.host               = "broker.emq.io";
    option.port               = "1883";
    option.keepalive_interval = 20;
    option.clean_session      = 1;

    plugin->finished = 0;
    pthread_mutex_init(&plugin->publish_mutex, NULL);
    pthread_mutex_init(&plugin->work_mutex, NULL);

    // Publish thread create
    int rc = pthread_create(&plugin->publish_thread_id, NULL,
                            (void *) publish_loop, plugin);
    if (0 != rc) {
        log_error("Create publish thread faild.");
        return -1;
    }
    pthread_detach(plugin->publish_thread_id);

    // Work thread create
    rc = pthread_create(&plugin->work_thread_id, NULL, (void *) work_loop,
                        plugin);
    if (0 != rc) {
        log_error("Create work thread faild.");
        return -1;
    }
    pthread_detach(plugin->work_thread_id);

    client_error error = paho_client_open(&option, &plugin->paho);
    if (ClientSuccess == error) {
        error =
            paho_client_subscribe(plugin->paho, "MQTT Examples", 0, NULL, NULL);
    }

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
    return 0;
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
