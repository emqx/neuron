#include "neuron.h"
#include <MQTTAsync.h>
#include <stdlib.h>

const neu_plugin_module_t neu_plugin_module;

static neu_plugin_t *mqtt_plugin_open(
    neu_adapter_t *adapter, const adapter_callbacks_t *callbacks)
{
    return NULL;
}

static int mqtt_plugin_close(neu_plugin_t *plugin)
{
    return 0;
}

static int mqtt_plugin_init(neu_plugin_t *plugin)
{
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