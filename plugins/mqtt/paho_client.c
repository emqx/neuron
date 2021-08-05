#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <MQTTAsync.h>
#include <neuron.h>

#include "option.h"
#include "paho_client.h"

#define PAYLOAD "Hello World!"

static char      server_url[200];
static option_t *extern_option;
static vector_t  subscribe_list;
static int       subscribe_token;

struct paho_client {
    MQTTAsync async;
};

typedef struct {
    char *           topic;
    int              qos;
    int              token;
    subscribe_handle handle;
    bool             subscribed;
} subscribe_tuple_t;

static void connection_lost_callback(void *context, char *cause)
{
    log_info("Connection lost.");
}

static int message_arrived_callback(void *context, char *topic, int len,
                                    MQTTAsync_message *message)
{
    log_debug("Message arrived topic:%s, message:%*s", topic,
              message->payloadlen, (char *) message->payload);
    MQTTAsync_freeMessage(&message);
    MQTTAsync_free(topic);
    return 1;
}

static void delivery_complete(void *context, MQTTAsync_token token)
{
    log_info("Message with token value %d delivery confirmed", token);
    return;
}

static void on_disconnect_failure(void *                 context,
                                  MQTTAsync_failureData *response)
{
    log_info("Disconnect failed, rc %d", response->code);
}

static void on_disconnect(void *context, MQTTAsync_successData *response)
{
    log_info("Successful disconnection");
}

static void on_subscribe(void *context, MQTTAsync_successData *response)
{
    log_info("Subscribe succeeded, token:%d", response->token);
}

static void on_subscribe_failure(void *context, MQTTAsync_failureData *response)
{
    log_info("Subscribe failed, rc %d", response->code);
}

static void on_connect_failure(void *context, MQTTAsync_failureData *response)
{
    log_info("Connect failed, rc %d", response->code);
}

void on_send_failure(void *context, MQTTAsync_failureData *response)
{
    log_info("Message send failed token %d error code %d", response->token,
             response->code);
}

void on_send(void *context, MQTTAsync_successData *response)
{
    log_info("Message with token value %d delivery confirmed", response->token);
}

static void on_connect(void *context, MQTTAsync_successData *response)
{
    log_info("Successful connection");
}

static void on_connected(void *context, char *cause)
{
    if (NULL != cause) {
        log_info("%s", cause);
    }

    // TODO:unsubscribe topic list
    // TODO:subscribe topic list
}

static client_error initialization()
{
    subscribe_token = 0;
    vector_setup(&subscribe_list, 64, sizeof(subscribe_tuple_t));
    if (!vector_is_initialized(&subscribe_list)) {
        return ClientSubscribeListInitialFailure;
    }
    return ClientSuccess;
}

client_error paho_client_open(option_t *option, paho_client_t **client)
{
    int rc = initialization();
    if (ClientSuccess != rc) {
        return rc;
    }

    // Setup MQTT
    extern_option       = option;
    *client             = (paho_client_t *) malloc(sizeof(paho_client_t));
    paho_client_t *paho = *client;

    memset(server_url, 0x00, sizeof(server_url));
    sprintf(server_url, "%s%s:%s", extern_option->connection,
            extern_option->host, extern_option->port);

    rc = MQTTAsync_create(&paho->async, server_url, extern_option->clientid,
                          MQTTCLIENT_PERSISTENCE_NONE, NULL);
    if (MQTTASYNC_SUCCESS != rc) {
        log_error("Failed to create client object, return code %d", rc);
        return rc;
    }

    rc = MQTTAsync_setCallbacks(paho->async, paho->async,
                                connection_lost_callback,
                                message_arrived_callback, delivery_complete);
    if (MQTTASYNC_SUCCESS != rc) {
        log_error("Failed to set callback, return code %d", rc);
        return rc;
    }

    MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
    conn_opts.MQTTVersion              = extern_option->MQTT_version;
    conn_opts.keepAliveInterval        = extern_option->keepalive_interval;
    conn_opts.cleansession             = extern_option->clean_session;
    conn_opts.onSuccess                = on_connect;
    conn_opts.onFailure                = on_connect_failure;
    conn_opts.context                  = paho->async;
    conn_opts.automaticReconnect       = 1;
    conn_opts.minRetryInterval         = 2;
    conn_opts.maxRetryInterval         = 365 * 24 * 60 * 60;

    int i;
    for (i = 0; i < 10; i++) {
        rc = MQTTAsync_connect(paho->async, &conn_opts);
        if (MQTTASYNC_SUCCESS == rc) {
            rc = MQTTAsync_setConnected(paho->async, (void *) paho->async,
                                        on_connected);
            log_info("Successfully start the request of connect.");
            return MQTTASYNC_SUCCESS;
        }
        log_error("Failed to start connect, return code %d.", rc);
        sleep(1);
    }
    return ClientConnectTimeout;
}

client_error paho_client_subscribe(paho_client_t *client, const char *topic,
                                   const int qos, subscribe_handle handle,
                                   void *context)
{
    subscribe_token++;
    subscribe_tuple_t *tuple =
        (subscribe_tuple_t *) malloc(sizeof(subscribe_tuple_t));
    tuple->topic      = strdup(topic);
    tuple->qos        = qos;
    tuple->token      = subscribe_token;
    tuple->handle     = handle;
    tuple->subscribed = false;
    vector_push_back(&subscribe_list, tuple);

    log_info("Subscribing to topic %s for client %s using QoS%d", topic, qos);

    MQTTAsync                 paho = client->async;
    MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
    opts.onSuccess                 = on_subscribe;
    opts.onFailure                 = on_subscribe_failure;
    opts.context                   = paho;
    opts.token                     = subscribe_token;
    int rc = MQTTAsync_subscribe(paho, extern_option->topic, extern_option->qos,
                                 &opts);
    if (rc != MQTTASYNC_SUCCESS) {
        log_error("Failed to start subscribe, return code %d", rc);
        vector_pop_back(&subscribe_list);

        if (NULL != tuple) {
            free(tuple->topic);
            free(tuple);
        }
        return rc;
    }
    return ClientSuccess;
}

client_error paho_client_unsubscribe(paho_client_t *client, const char *topic)
{
    int                       rc;
    MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
    opts.token                     = 0;
    rc = MQTTAsync_unsubscribe(client->async, topic, &opts);
    // TODO: delete from susbcribe_list
    return rc;
}

client_error paho_client_publish(paho_client_t *client, const char *topic,
                                 int qos, unsigned char *payload, size_t len)
{
    MQTTAsync                 paho = client->async;
    MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
    MQTTAsync_message         msg  = MQTTAsync_message_initializer;
    int                       rc;

    printf("Successful connection\n");
    opts.onSuccess = on_send;
    opts.onFailure = on_send_failure;
    opts.context   = paho;
    msg.payload    = payload;
    msg.payloadlen = len;
    msg.qos        = qos;
    msg.retained   = 0;

    rc = MQTTAsync_sendMessage(paho, topic, &msg, &opts);
    if (MQTTASYNC_SUCCESS != rc) {
        log_error("Failed to start sendMessage, return code %d", rc);
        return rc;
    }
    return ClientSuccess;
}

client_error paho_client_close(paho_client_t *client)
{
    MQTTAsync_disconnectOptions opts = MQTTAsync_disconnectOptions_initializer;
    opts.onSuccess                   = on_disconnect;
    opts.onFailure                   = on_disconnect_failure;

    if (NULL != client) {
        MQTTAsync_disconnect(client->async, &opts);
        MQTTAsync_destroy(&client->async);
        free(client);
    }
    return ClientSuccess;
}