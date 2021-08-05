#include <stdio.h>
#include <stdlib.h>

#include <MQTTAsync.h>
#include <neuron.h>

#include "option.h"
#include "paho_client.h"

#define PAYLOAD "Hello World!"

static char      server_url[200];
static option_t *extern_option;

struct paho_client {
    MQTTAsync async;
};

static void connection_lost_callback(void *context, char *cause)
{
    log_info("Connection lost.");
}

static int message_arrived_callback(void *context, char *topicName,
                                    int topicLen, MQTTAsync_message *message)
{
    printf("Message arrived\n");
    printf("     topic: %s\n", topicName);
    printf("   message: %.*s\n", message->payloadlen,
           (char *) message->payload);
    MQTTAsync_freeMessage(&message);
    MQTTAsync_free(topicName);
    return 1;
}

static void delivery_complete(void *context, MQTTAsync_token token)
{
    return;
}

static void on_disconnect_failure(void *                 context,
                                  MQTTAsync_failureData *response)
{
    log_info("Disconnect failed, rc %d\n", response->code);
    // disc_finished = 1;
}

static void on_disconnect(void *context, MQTTAsync_successData *response)
{
    log_info("Successful disconnection\n");
    // disc_finished = 1;
}

static void on_subscribe(void *context, MQTTAsync_successData *response)
{
    log_info("Subscribe succeeded\n");
    // subscribed = 1;
}

static void on_subscribe_failure(void *context, MQTTAsync_failureData *response)
{
    log_info("Subscribe failed, rc %d\n", response->code);
    // finished = 1;
}

static void on_connect_failure(void *context, MQTTAsync_failureData *response)
{
    log_info("Connect failed, rc %d\n", response->code);
    // finished = 1;
}

static void on_connect(void *context, MQTTAsync_successData *response)
{
    MQTTAsync                 client = (MQTTAsync) context;
    MQTTAsync_responseOptions opts   = MQTTAsync_responseOptions_initializer;

    printf("Successful connection\n");

    // printf("Subscribing to topic %s\nfor client %s using QoS%d\n\n" "Press
    // Q<Enter> to quit\n\n", TOPIC, CLIENTID, QOS);

    opts.onSuccess = on_subscribe;
    opts.onFailure = on_subscribe_failure;
    opts.context   = client;
    int rc         = MQTTAsync_subscribe(client, extern_option->topic,
                                 extern_option->qos, &opts);
    if (rc != MQTTASYNC_SUCCESS) {
        printf("Failed to start subscribe, return code %d\n", rc);
        // finished = 1;
    }
}

static void on_connected(void *context, char *cause)
{
    if (NULL != cause) {
        log_info("%s", cause);
    }

    // TODO:unsubscribe topic list
    // TODO:subscribe topic list
}

client_error paho_client_open(option_t *option, paho_client_t **client)
{
    extern_option       = option;
    *client             = (paho_client_t *) malloc(sizeof(paho_client_t));
    paho_client_t *paho = *client;

    memset(server_url, 0x00, sizeof(server_url));
    sprintf(server_url, "%s%s:%s", extern_option->connection,
            extern_option->host, extern_option->port);

    int rc = MQTTAsync_create(&paho->async, server_url, extern_option->clientid,
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

client_error paho_client_close(paho_client_t *client)
{
    if (NULL != client) {
        MQTTAsync_destroy(&client->async);
        free(client);
    }
    return 0;
}