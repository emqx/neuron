#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <MQTTAsync.h>
#include <neuron.h>

#include "option.h"
#include "paho_client.h"

#define UNUSED(x) (void) (x)
// #define DEBUG

struct subscribe_tuple {
    neu_list_node             node;
    MQTTAsync_responseOptions subscribe_option;
    MQTTAsync_responseOptions unsubscribe_option;
    char *                    topic;
    int                       qos;
    int                       token;
    subscribe_handle          handle;
    bool                      subscribed;
    paho_client_t *           client;
};

struct paho_client {
    MQTTAsync_connectOptions conn_options;
    char                     server_url[200];
    option_t *               option;
    neu_list                 subscribe_list;
    int                      subscribe_token;
    MQTTAsync                async;
    void *                   user_data;
};

static void connection_lost_callback(void *context, char *cause)
{
    log_info("Connection lost, reconnecting");

    paho_client_t *client = (paho_client_t *) context;
    paho_client_connect(client);
    UNUSED(cause);
}

static int message_arrived_callback(void *context, char *topic, int len,
                                    MQTTAsync_message *message)
{
    paho_client_t *    client = (paho_client_t *) context;
    subscribe_tuple_t *item;
    NEU_LIST_FOREACH(&client->subscribe_list, item)
    {
        if (0 == strcmp(item->topic, topic) && NULL != item->handle) {
            item->handle(topic, len, message->payload, message->payloadlen,
                         client->user_data);
        }
    }

    MQTTAsync_freeMessage(&message);
    MQTTAsync_free(topic);
    return 1;
}

static void delivery_complete(void *context, MQTTAsync_token token)
{
    UNUSED(context);
    log_info("Message with token value %d delivery confirmed", token);
}

static void on_disconnect_failure(void *                 context,
                                  MQTTAsync_failureData *response)
{
    UNUSED(context);
    log_info("Disconnect failed, rc %d", response->code);
}

static void on_disconnect(void *context, MQTTAsync_successData *response)
{
    UNUSED(context);
    UNUSED(response);
    log_info("Successful disconnection");
}

static void on_subscribe(void *context, MQTTAsync_successData *response)
{
    UNUSED(context);
    UNUSED(response);
}

static void on_subscribe_failure(void *context, MQTTAsync_failureData *response)
{
    UNUSED(context);
    log_info("Subscribe failed, rc %d", response->code);
}

static void on_unsubscribe(void *context, MQTTAsync_successData *response)
{
    UNUSED(context);
    log_info("Unsubscribe succeeded, token:%d", response->token);
}

static void on_unsubscribe_failure(void *                 context,
                                   MQTTAsync_failureData *response)
{
    UNUSED(context);
    log_info("Unsubscribe failed, rc %d", response->code);
}

static void on_connect_failure(void *context, MQTTAsync_failureData *response)
{
    UNUSED(context);
    log_info("Connect failed, rc %d", response->code);
}

static void on_send_failure(void *context, MQTTAsync_failureData *response)
{
    UNUSED(context);
    log_info("Message send failed token %d error code %d", response->token,
             response->code);
}

static void on_send(void *context, MQTTAsync_successData *response)
{
    UNUSED(context);
    log_info("Message with token value %d delivery confirmed", response->token);
}

static void on_connect_subscribe_all_topic(void *context)
{
    paho_client_t *    client = (paho_client_t *) context;
    subscribe_tuple_t *item;
    NEU_LIST_FOREACH(&client->subscribe_list, item)
    {
        paho_client_subscribe_send(client, item);
    }
}

static void on_connect(void *context, MQTTAsync_successData *response)
{
    UNUSED(response);
    log_info("Successful connection");
    on_connect_subscribe_all_topic(context);
}

static void on_connected(void *context, char *cause)
{
    if (NULL == cause) {
        return;
    }

    log_info("%s", cause);

    if (0 == strcmp(cause, "connect onSuccess called")) { }

    if (0 == strcmp(cause, "automatic reconnect")) {
        on_connect_subscribe_all_topic((paho_client_t *) context);
    }
}

static void on_disconnected(void *context, MQTTProperties *properties,
                            enum MQTTReasonCodes reason_code)
{
    UNUSED(context);
    UNUSED(properties);
    UNUSED(reason_code);
}

static client_error paho_client_connect_option_bind(paho_client_t *client)
{
    MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
    conn_opts.MQTTVersion              = client->option->MQTT_version;
    conn_opts.keepAliveInterval        = client->option->keepalive_interval;
    conn_opts.cleansession             = client->option->clean_session;
    conn_opts.onSuccess                = on_connect;
    conn_opts.onFailure                = on_connect_failure;
    conn_opts.context                  = client;
    conn_opts.automaticReconnect       = 1;
    conn_opts.minRetryInterval         = 2;
    conn_opts.maxRetryInterval         = 365 * 24 * 60 * 60;
    client->conn_options               = conn_opts;
    return ClientSuccess;
}

static client_error paho_client_will_option_bind(paho_client_t *client)
{
    UNUSED(client);
    return ClientSuccess;
}

static client_error paho_client_ssl_option_bind(paho_client_t *client)
{
    UNUSED(client);
    return ClientSuccess;
}

void paho_client_connect_option_bind_debug(paho_client_t *client)
{
    log_debug("MQTTVersion %d", client->conn_options.MQTTVersion);
    log_debug("keepAliveInterval %d", client->conn_options.keepAliveInterval);
    log_debug("cleansession %d", client->conn_options.cleansession);
    log_debug("onSuccess %x", client->conn_options.onSuccess);
    log_debug("onFailure %x", client->conn_options.onFailure);
    log_debug("context %x", client->conn_options.context);
    log_debug("automaticReconnect %d", client->conn_options.automaticReconnect);
    log_debug("minRetryInterval %d", client->conn_options.minRetryInterval);
    log_debug("maxRetryInterval %d", client->conn_options.maxRetryInterval);
}

static client_error paho_client_field_init(paho_client_t *client)
{
    client->subscribe_token = 0;

    NEU_LIST_INIT(&client->subscribe_list, subscribe_tuple_t, node);
    return ClientSuccess;
}

paho_client_t *paho_client_create(option_t *option, void *context)
{
    paho_client_t *client = (paho_client_t *) malloc(sizeof(paho_client_t));
    if (NULL == client) {
        log_error("Failed to malloc client memory");
        return NULL;
    }

    client->option    = option;
    client->user_data = context;

    memset(client->server_url, 0x00, sizeof(client->server_url));
    sprintf(client->server_url, "%s%s:%s", client->option->connection,
            client->option->host, client->option->port);

    int rc = MQTTAsync_create(&client->async, client->server_url,
                              client->option->clientid,
                              MQTTCLIENT_PERSISTENCE_NONE, NULL);
    if (MQTTASYNC_SUCCESS != rc) {
        log_error("Failed to create client object, return code %d", rc);
        goto error;
    }

    rc = MQTTAsync_setCallbacks(client->async, client, connection_lost_callback,
                                message_arrived_callback, delivery_complete);
    if (MQTTASYNC_SUCCESS != rc) {
        log_error("Failed to set callback, return code %d", rc);
        goto error;
    }

    rc = paho_client_connect_option_bind(client);
#ifdef DEBUG
    paho_client_connect_option_bind_debug(client);
#endif
    if (ClientSuccess != rc) {
        log_error("Failed to bind connection options %d", rc);
        goto error;
    }

    rc = paho_client_ssl_option_bind(client);

    rc = paho_client_will_option_bind(client);

    rc = paho_client_field_init(client);
    if (ClientSuccess != rc) {
        log_error("Failed to init client %d", rc);
        goto error;
    }
    return client;

error:
    free(client);
    return NULL;
}

static client_error paho_client_connect_set_callback(paho_client_t *client)
{
    if (NULL == client) {
        log_error("The client is not initialized");
        return ClientIsNULL;
    }

    MQTTAsync_setConnected(client->async, (void *) client, on_connected);
    MQTTAsync_setDisconnected(client->async, (void *) client, on_disconnected);
    return MQTTASYNC_SUCCESS;
}

client_error paho_client_connect(paho_client_t *client)
{
    if (NULL == client) {
        log_error("The client is not initialized");
        return ClientIsNULL;
    }

    int rc = MQTTAsync_connect(client->async, &client->conn_options);
    if (MQTTASYNC_SUCCESS == rc) {
        log_info("Successfully start the request of connect.");
        return MQTTASYNC_SUCCESS;
    }

    log_error("Failed to start connect, return code %d.", rc);
    return ClientConnectFailure;
}

client_error paho_client_disconnect(paho_client_t *client)
{
    MQTTAsync_disconnectOptions opts = MQTTAsync_disconnectOptions_initializer;
    opts.onSuccess                   = on_disconnect;
    opts.onFailure                   = on_disconnect_failure;

    subscribe_tuple_t *item;
    NEU_LIST_FOREACH(&client->subscribe_list, item)
    {
        paho_client_unsubscribe(client, item->topic);
    }

    if (NULL != client) {
        MQTTAsync_disconnect(client->async, &opts);
        MQTTAsync_destroy(&client->async);
    }
    return ClientSuccess;
}

client_error paho_client_destroy(paho_client_t *client)
{
    while (!neu_list_empty(&client->subscribe_list)) {
        subscribe_tuple_t *tuple = neu_list_first(&client->subscribe_list);
        neu_list_remove(&client->subscribe_list, tuple);
    }

    free(client);
    return ClientSuccess;
}

client_error paho_client_open(option_t *option, void *context,
                              paho_client_t **p_client)
{
    paho_client_t *client = paho_client_create(option, context);
    if (NULL == client) {
        return ClientIsNULL;
    }

    client_error rc = paho_client_connect(client);
    paho_client_connect_set_callback(client);
    *p_client = client;
    return rc;
}

client_error paho_client_is_connected(paho_client_t *client)
{
    int rc = MQTTAsync_isConnected(client->async);
    return 0 != rc ? ClientSuccess : ClientConnectFailure;
}

static client_error paho_client_subscribe_option_bind(subscribe_tuple_t *tuple)
{
    MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
    opts.onSuccess                 = on_subscribe;
    opts.onFailure                 = on_subscribe_failure;
    opts.context                   = tuple->client;
    opts.token                     = tuple->token;
    tuple->subscribe_option        = opts;
    return ClientSuccess;
}

subscribe_tuple_t *paho_client_subscribe_create(paho_client_t *        client,
                                                const char *           topic,
                                                const int              qos,
                                                const subscribe_handle handle)
{
    subscribe_tuple_t *tuple =
        (subscribe_tuple_t *) malloc(sizeof(subscribe_tuple_t));

    if (NULL == tuple) {
        log_error("Failed to malloc client memory");
        return NULL;
    }

    tuple->topic      = strdup(topic);
    tuple->qos        = qos;
    tuple->token      = client->subscribe_token;
    tuple->handle     = handle;
    tuple->subscribed = false;
    tuple->client     = client;

    client_error rc = paho_client_subscribe_option_bind(tuple);
    if (ClientSuccess != rc) {
        log_error("Failed to bind subscribe options %d", rc);
        return NULL;
    }
    return tuple;
}

client_error paho_client_subscribe_destroy(subscribe_tuple_t *tuple)
{
    if (NULL != tuple) {
        if (NULL != tuple->topic) {
            free(tuple->topic);
        }
        free(tuple);
    }
    return ClientSuccess;
}

client_error paho_client_subscribe_send(paho_client_t *    client,
                                        subscribe_tuple_t *tuple)
{
    int rc = MQTTAsync_subscribe(client->async, tuple->topic, tuple->qos,
                                 &tuple->subscribe_option);
    if (rc != MQTTASYNC_SUCCESS) {
        log_error("Failed to start subscribe, return code %d", rc);
        return rc;
    }
    return ClientSuccess;
}

client_error paho_client_subscribe_add(paho_client_t *    client,
                                       subscribe_tuple_t *tuple)
{
    NEU_LIST_NODE_INIT(&tuple->node);
    neu_list_append(&client->subscribe_list, tuple);
    return ClientSuccess;
}

client_error paho_client_subscribe_remove(paho_client_t *client,
                                          const char *   topic)
{
    subscribe_tuple_t *item;
    subscribe_tuple_t *tuple;
    NEU_LIST_FOREACH(&client->subscribe_list, item)
    {
        if (0 == strcmp(item->topic, topic)) {
            tuple = item;
        }
    }
    neu_list_remove(&client->subscribe_list, tuple);
    return ClientSuccess;
}

client_error paho_client_subscribe(paho_client_t *client, const char *topic,
                                   const int qos, subscribe_handle handle)
{
    if (NULL == client) {
        return ClientIsNULL;
    }

    log_info("Subscribing to topic %s for using QoS%d", topic, qos);

    subscribe_tuple_t *tuple =
        paho_client_subscribe_create(client, topic, qos, handle);
    if (NULL == tuple) {
        return ClientSubscribeFailure;
    }

    client_error rc = paho_client_subscribe_add(client, tuple);
    if (ClientSuccess != rc) {
        return rc;
    }

    rc = paho_client_is_connected(client);
    if (ClientSuccess != rc) {
        return rc;
    }

    rc = paho_client_subscribe_send(client, tuple);
    if (ClientSuccess != rc) {
        return rc;
    }
    return ClientSuccess;
}

static client_error
paho_client_unsubscribe_option_bind(subscribe_tuple_t *tuple)
{
    MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
    opts.onSuccess                 = on_unsubscribe;
    opts.onFailure                 = on_unsubscribe_failure;
    opts.context                   = tuple->client;
    opts.token                     = tuple->token;
    tuple->unsubscribe_option      = opts;
    return ClientSuccess;
}

subscribe_tuple_t *paho_client_unsubscribe_create(paho_client_t *client,
                                                  const char *   topic)
{
    subscribe_tuple_t *item;
    NEU_LIST_FOREACH(&client->subscribe_list, item)
    {
        if (0 == strcmp(item->topic, topic)) {
            paho_client_unsubscribe_option_bind(item);
        }
        return item;
    }

    log_error("Cant find topic %s", topic);
    return NULL;
}

client_error paho_client_unsubscribe_send(paho_client_t *    client,
                                          subscribe_tuple_t *tuple)
{
    return MQTTAsync_unsubscribe(client->async, tuple->topic,
                                 &tuple->unsubscribe_option);
}

client_error paho_client_unsubscribe(paho_client_t *client, const char *topic)
{
    if (NULL == client) {
        return ClientIsNULL;
    }

    subscribe_tuple_t *tuple = paho_client_unsubscribe_create(client, topic);
    if (NULL == tuple) {
        return ClientUnsubscribeFailure;
    }

    client_error rc = paho_client_is_connected(client);
    if (ClientSuccess != rc) {
        return rc;
    }

    paho_client_unsubscribe_send(client, tuple);

    rc = paho_client_subscribe_remove(client, topic);
    if (ClientSuccess != rc) {
        return rc;
    }

    return ClientSuccess;
}

client_error paho_client_publish(paho_client_t *client, const char *topic,
                                 int qos, unsigned char *payload, size_t len)
{
    if (NULL == client) {
        return ClientIsNULL;
    }

    client_error error = paho_client_is_connected(client);
    if (ClientSuccess != error) {
        paho_client_connect(client);
        return error;
    }

    MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
    MQTTAsync_message         msg  = MQTTAsync_message_initializer;

    opts.onSuccess = on_send;
    opts.onFailure = on_send_failure;
    opts.context   = client;

    msg.payload    = payload;
    msg.payloadlen = len;
    msg.qos        = qos;
    msg.retained   = 0;

    error = MQTTAsync_sendMessage(client->async, topic, &msg, &opts);
    if (MQTTASYNC_SUCCESS != error) {
        log_error("Failed to start sendMessage, return code %d", error);
        return error;
    }
    return ClientSuccess;
}

client_error paho_client_close(paho_client_t *client)
{
    if (NULL == client) {
        return ClientIsNULL;
    }

    client_error rc = paho_client_disconnect(client);
    rc              = paho_client_destroy(client);
    return rc;
}
