#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <MQTTAsync.h>
#include <neuron.h>

#include "option.h"
#include "paho_client.h"

#define UNUSED(x) (void) (x)
#define DEBUG

static vector_t subscribe_list;

struct subscribe_tuple {
    MQTTAsync_responseOptions subscribe_option;
    char *                    topic;
    int                       qos;
    int                       token;
    subscribe_handle          handle;
    bool                      subscribed;
    paho_client_t *           client;
};

struct paho_client {
    char                     server_url[200];
    option_t *               option;
    vector_t                 subscribe_vector;
    int                      subscribe_token;
    MQTTAsync_connectOptions conn_options;
    MQTTAsync                async;
    void *                   user_data;
};

static void connection_lost_callback(void *context, char *cause)
{
    log_info("Connection lost, reconnecting");

    paho_client_t *client = (paho_client_t *) context;
    if (NULL == client) {
        return;
    }

    int rc = MQTTAsync_connect(client->async, &client->conn_options);
    if (rc != MQTTASYNC_SUCCESS) {
        printf("Failed to start connect, return code %d\n", rc);
    }
}

static int message_arrived_callback(void *context, char *topic, int len,
                                    MQTTAsync_message *message)
{
    log_debug("Message arrived topic:%s, message:%*s", topic,
              message->payloadlen, (char *) message->payload);

    iterator_t iterator = vector_begin(&subscribe_list);
    iterator_t last     = vector_end(&subscribe_list);
    for (; !iterator_equals(&iterator, &last); iterator_increment(&iterator)) {
        subscribe_tuple_t *item = iterator_get(&iterator);
        if (0 == strcmp(item->topic, topic) && item->subscribed &&
            NULL != item->handle) {
            item->handle(topic, len, message->payload, message->payloadlen,
                         context);
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
    return;
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
    log_info("Subscribe succeeded, token:%d", response->token);

    iterator_t iterator = vector_begin(&subscribe_list);
    iterator_t last     = vector_end(&subscribe_list);
    for (; !iterator_equals(&iterator, &last); iterator_increment(&iterator)) {
        subscribe_tuple_t *item = iterator_get(&iterator);
        if (response->token == item->token) {
            item->subscribed = true;
        }
    }
}

static void on_subscribe_failure(void *context, MQTTAsync_failureData *response)
{
    UNUSED(context);
    log_info("Subscribe failed, rc %d", response->code);
}

static void on_connect_failure(void *context, MQTTAsync_failureData *response)
{
    UNUSED(context);
    log_info("Connect failed, rc %d", response->code);
}

void on_send_failure(void *context, MQTTAsync_failureData *response)
{
    UNUSED(context);
    log_info("Message send failed token %d error code %d", response->token,
             response->code);
}

void on_send(void *context, MQTTAsync_successData *response)
{
    UNUSED(context);
    log_info("Message with token value %d delivery confirmed", response->token);
}

static void on_connect(void *context, MQTTAsync_successData *response)
{
    UNUSED(context);
    UNUSED(response);
    log_info("Successful connection");
}

static void on_connected(void *context, char *cause)
{
    paho_client_t *paho = (paho_client_t *) context;

    if (NULL != cause) {
        log_info("%s", cause);
    }

    // Unsubscribe all topic
    iterator_t iterator = vector_begin(&subscribe_list);
    iterator_t last     = vector_end(&subscribe_list);
    for (; !iterator_equals(&iterator, &last); iterator_increment(&iterator)) {
        subscribe_tuple_t *item = iterator_get(&iterator);
        int                rc   = paho_client_unsubscribe(paho, item->topic);
        if (0 == rc) {
            item->subscribed = false;
        }
    }

    // TODO: Subscribe all topic
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
    conn_opts.context                  = client->async;
    conn_opts.automaticReconnect       = 1;
    conn_opts.minRetryInterval         = 2;
    conn_opts.maxRetryInterval         = 365 * 24 * 60 * 60;
    client->conn_options               = conn_opts;
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

    vector_setup(&client->subscribe_vector, 64, sizeof(subscribe_tuple_t));
    if (!vector_is_initialized(&client->subscribe_vector)) {
        log_error("Failed to initialize subscribe vector");
        return ClientSubscribeListInitialFailure;
    }
    return ClientSuccess;
}

paho_client_t *paho_client_create(option_t *option)
{
    paho_client_t *client = (paho_client_t *) malloc(sizeof(paho_client_t));
    if (NULL == client) {
        log_error("Failed to malloc client memory");
        return NULL;
    }

    client->option = option;

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

client_error paho_client_connect(paho_client_t *client)
{
    if (NULL == client) {
        log_error("The client is not initialized");
        return ClientIsNULL;
    }

    int rc = MQTTAsync_connect(client->async, &client->conn_options);
    if (MQTTASYNC_SUCCESS == rc) {
        rc = MQTTAsync_setConnected(client->async, (void *) client,
                                    on_connected);
        rc = MQTTAsync_setDisconnected(client->async, (void *) client,
                                       on_disconnected);
        log_info("Successfully start the request of connect.");
        return MQTTASYNC_SUCCESS;
    }

    log_error("Failed to start connect, return code %d.", rc);
    return ClientConnectFailure;
}

client_error paho_client_disconnect(paho_client_t *client)
{
    if (NULL == client) {
        log_error("The client is not initialized");
        return ClientIsNULL;
    }

    MQTTAsync_disconnectOptions opts = MQTTAsync_disconnectOptions_initializer;
    opts.onSuccess                   = on_disconnect;
    opts.onFailure                   = on_disconnect_failure;

    // Unsubscribe all topic
    VECTOR_FOR_EACH(&client->subscribe_vector, i)
    {
        subscribe_tuple_t tuple = ITERATOR_GET_AS(subscribe_tuple_t, &i);
        paho_client_unsubscribe(client, tuple.topic);
    }

    if (NULL != client) {
        MQTTAsync_disconnect(client->async, &opts);
        MQTTAsync_destroy(&client->async);
    }
    return ClientSuccess;
}

client_error paho_client_destroy(paho_client_t *client)
{
    if (NULL == client) {
        log_error("The client is not initialized");
        return ClientIsNULL;
    }

    if (vector_is_initialized(&client->subscribe_vector)) {
        vector_clear(&client->subscribe_vector);
        vector_destroy(&client->subscribe_vector);
    }

    free(client);
    return ClientSuccess;
}

client_error paho_client_open(option_t *option, paho_client_t **client_point)
{
    paho_client_t *client = paho_client_create(option);
    if (NULL == client) {
        return ClientIsNULL;
    }

    client_error rc = paho_client_connect(client);
    *client_point   = client;
    return rc;
}

client_error paho_client_is_connection(paho_client_t *client)
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
    iterator_t iterator = vector_begin(&client->subscribe_vector);
    iterator_t last     = vector_end(&client->subscribe_vector);
    for (; !iterator_equals(&iterator, &last); iterator_increment(&iterator)) {
        subscribe_tuple_t *item = iterator_get(&iterator);
        if (0 == strcmp(tuple->topic, item->topic)) {
            log_error("Failed to add subscribe to vector, topic existed");
            return ClientSubscribeAddListRepeat;
        }
    }

    int rc = vector_push_back(&subscribe_list, tuple);
    if (0 != rc) {
        log_error("Failed to add subscribe to vector, vector exception");
        return ClientSubscribeAddListFailure;
    }
    return ClientSuccess;
}

client_error paho_client_subscribe_remove(paho_client_t *client,
                                          const char *   topic)
{
    int i = 0;

    iterator_t iterator = vector_begin(&client->subscribe_vector);
    iterator_t last     = vector_end(&client->subscribe_vector);
    for (; !iterator_equals(&iterator, &last); iterator_increment(&iterator)) {
        subscribe_tuple_t *item = iterator_get(&iterator);
        if (0 == strcmp(topic, item->topic)) {
            vector_erase(&client->subscribe_vector, i);
            paho_client_subscribe_destroy(item);
            break;
        }
        i++;
    }
    return ClientSuccess;
}

client_error paho_client_subscribe(paho_client_t *client, const char *topic,
                                   const int qos, subscribe_handle handle,
                                   void *context)
{
    subscribe_tuple_t *tuple =
        paho_client_subscribe_create(client, topic, qos, handle);
    if (NULL == tuple) {
        return ClientSubscribeFailure;
    }

    client_error rc = paho_client_subscribe_add(client, tuple);
    if (ClientSuccess != rc) {
        return rc;
    }

    rc = paho_client_subscribe_send(client, tuple);
    if (ClientSuccess != rc) {
        return rc;
    }

    return ClientSuccess;
}

// client_error paho_client_subscribe2(paho_client_t *client, const char *topic,
//                                     const int qos, subscribe_handle handle,
//                                     void *context)
// {
//     UNUSED(context);

//     if (NULL == client) {
//         return ClientIsNULL;
//     }

//     subscribe_token++;
//     subscribe_tuple_t *tuple =
//         (subscribe_tuple_t *) malloc(sizeof(subscribe_tuple_t));
//     tuple->topic      = strdup(topic);
//     tuple->qos        = qos;
//     tuple->token      = subscribe_token;
//     tuple->handle     = handle;
//     tuple->subscribed = false;
//     vector_push_back(&subscribe_list, tuple);

//     log_info("Subscribing to topic %s for using QoS%d", topic, qos);
//     MQTTAsync paho = client->async;

//     MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;

//     opts.onSuccess = on_subscribe;
//     opts.onFailure = on_subscribe_failure;
//     opts.context   = paho;
//     opts.token     = subscribe_token;
//     int rc = MQTTAsync_subscribe(paho, extern_option->topic,
//     extern_option->qos,
//                                  &opts);
//     if (rc != MQTTASYNC_SUCCESS) {
//         log_error("Failed to start subscribe, return code %d", rc);
//         vector_pop_back(&subscribe_list);

//         if (NULL != tuple) {
//             free(tuple->topic);
//             free(tuple);
//         }
//         return rc;
//     }

//     return ClientSubscribeTimeout;
// }

client_error paho_client_unsubscribe_create(paho_client_t *client,
                                            const char *   topic);

client_error paho_client_unsubscribe_send(paho_client_t *client,
                                          const char *   topic);

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

    // Unsubscribe all topic
    VECTOR_FOR_EACH(&subscribe_list, i)
    {
        subscribe_tuple_t tuple = ITERATOR_GET_AS(subscribe_tuple_t, &i);
        paho_client_unsubscribe(client, tuple.topic);
    }

    if (vector_is_initialized(&subscribe_list)) {
        vector_clear(&subscribe_list);
        vector_destroy(&subscribe_list);
    }

    if (NULL != client) {
        MQTTAsync_disconnect(client->async, &opts);
        MQTTAsync_destroy(&client->async);
        free(client);
    }
    return ClientSuccess;
}
