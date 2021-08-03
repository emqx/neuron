#include <MQTTAsync.h>
#include <stdlib.h>

#include "neuron.h"

const neu_plugin_module_t neu_plugin_module;

struct neu_plugin {
    neu_plugin_common_t common;
    MQTTAsync           client;
};

static neu_plugin_t *mqtt_plugin_open(
    neu_adapter_t *adapter, const adapter_callbacks_t *callbacks)
{
    if (NULL == adapter || NULL == callbacks) {
        return NULL;
    }

    neu_plugin_t *plugin;
    plugin = (neu_plugin_t *) malloc(sizeof(neu_plugin_t));
    if (NULL == plugin) {
        log_error(
            "Failed to allocate plugin %s", neu_plugin_module.module_name);
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

static void connection_lost_callback(void *context, char *cause)
{
    MQTTAsync                client    = (MQTTAsync) context;
    MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
    int                      rc;

    printf("\nConnection lost\n");
    if (cause)
        printf("     cause: %s\n", cause);

    printf("Reconnecting\n");
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession      = 1;
    if ((rc = MQTTAsync_connect(client, &conn_opts)) != MQTTASYNC_SUCCESS) {
        printf("Failed to start connect, return code %d\n", rc);
        // finished = 1;
    }
}

static int message_arrived_callback(
    void *context, char *topicName, int topicLen, MQTTAsync_message *message)
{
    printf("Message arrived\n");
    printf("     topic: %s\n", topicName);
    printf(
        "   message: %.*s\n", message->payloadlen, (char *) message->payload);
    MQTTAsync_freeMessage(&message);
    MQTTAsync_free(topicName);
    return 1;
}

void on_disconnect_failure(void *context, MQTTAsync_failureData *response)
{
    printf("Disconnect failed, rc %d\n", response->code);
    // disc_finished = 1;
}

void on_disconnect(void *context, MQTTAsync_successData *response)
{
    printf("Successful disconnection\n");
    // disc_finished = 1;
}

void on_subscribe(void *context, MQTTAsync_successData *response)
{
    printf("Subscribe succeeded\n");
    // subscribed = 1;
}

void on_subscribe_failure(void *context, MQTTAsync_failureData *response)
{
    printf("Subscribe failed, rc %d\n", response->code);
    // finished = 1;
}

void on_connectFailure(void *context, MQTTAsync_failureData *response)
{
    printf("Connect failed, rc %d\n", response->code);
    // finished = 1;
}

void on_connect(void *context, MQTTAsync_successData *response)
{
    MQTTAsync                 client = (MQTTAsync) context;
    MQTTAsync_responseOptions opts   = MQTTAsync_responseOptions_initializer;
    int                       rc;

    printf("Successful connection\n");

    // printf("Subscribing to topic %s\nfor client %s using QoS%d\n\n" "Press
    // Q<Enter> to quit\n\n", TOPIC, CLIENTID, QOS);
    opts.onSuccess = on_subscribe;
    opts.onFailure = on_subscribe_failure;
    opts.context   = client;
    // if ((rc = MQTTAsync_subscribe(client, TOPIC, QOS, &opts)) !=
    //     MQTTASYNC_SUCCESS) {
    //     printf("Failed to start subscribe, return code %d\n", rc);
    //     // finished = 1;
    // }
}

static int mqtt_plugin_init(neu_plugin_t *plugin)
{
    MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
    MQTTAsync_message        pub_msg   = MQTTAsync_message_initializer;

    int rc;
    rc = MQTTAsync_create(&plugin->client, "tcp://broker.emqx.io:1883",
        "neuron-lite-mqtt-plugin", MQTTCLIENT_PERSISTENCE_NONE, NULL);
    if (MQTTASYNC_SUCCESS != rc) {
        log_error("Failed to create client object, return code %d", rc);
        return -1;
    }

    rc = MQTTAsync_setCallbacks(plugin->client, plugin->client,
        connection_lost_callback, message_arrived_callback, NULL);
    if (MQTTASYNC_SUCCESS != rc) {
        log_error("Failed to set callback, return code %d", rc);
        return -1;
    }

    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession      = 1;
    conn_opts.onSuccess         = on_connect;
    conn_opts.onFailure         = on_connectFailure;
    conn_opts.context           = plugin->client;

    rc = MQTTAsync_connect(plugin->client, &conn_opts);
    if (MQTTASYNC_SUCCESS != rc) {
        log_error("Failed to start connect, return code %d", rc);
        return -1;
    }

    log_info("Succeed to start MQTT client %d", rc);
    return 0;
}

static int mqtt_plugin_uninit(neu_plugin_t *plugin)
{
    return 0;
}

static int mqtt_plugin_config(neu_plugin_t *plugin, neu_config_t *configs)
{
    return 0;
}

static int mqtt_plugin_request(neu_plugin_t *plugin, neu_request_t *req)
{
    return 0;
}

static int mqtt_plugin_event_reply(
    neu_plugin_t *plugin, neu_event_reply_t *reply)
{
    return 0;
}

static const neu_plugin_intf_funs_t plugin_intf_funs = { .open =
                                                             mqtt_plugin_open,
    .init        = mqtt_plugin_init,
    .uninit      = mqtt_plugin_uninit,
    .config      = mqtt_plugin_config,
    .request     = mqtt_plugin_request,
    .event_reply = mqtt_plugin_event_reply };

const neu_plugin_module_t neu_plugin_module = { .version =
                                                    NEURON_PLUGIN_VER_1_0,
    .module_name  = "neuron-mqtt-plugin",
    .module_descr = "Neuron northbound MQTT communication plugin",
    .intf_funs    = &plugin_intf_funs };

int test()
{
    MQTTAsync                client;
    MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;

    int rc;
    int ch;
    return 0;
}