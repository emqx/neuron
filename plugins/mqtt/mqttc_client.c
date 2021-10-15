
/**
 * @file
 * A simple subscriber program that performs automatic reconnections.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <mqtt.h>
#include <neuron.h>

#include "mqttc_client.h"
#include "posix_sockets.h"

#define UNUSED(x) (void) (x)
#define INTERVAL 100000U
#define SEND_BUF_SIZE 2048
#define RECV_BUF_SIZE 2048

struct reconnect_state_t {
    const char *    hostname;
    const char *    port;
    const char *    client_id;
    const char *    topic;
    mqttc_client_t *client;
    uint8_t *       send_buf;
    size_t          send_buf_sz;
    uint8_t *       recv_buf;
    size_t          recv_buf_sz;
};

struct mqttc_client {
    option_t *               option;
    neu_list                 subscribe_list;
    struct mqtt_client       mqttc;
    struct reconnect_state_t reconnect_state;
    uint8_t                  send_buf[SEND_BUF_SIZE];
    uint8_t                  recv_buf[RECV_BUF_SIZE];
    pthread_t                client_daemon;
    void *                   user_data;
};

struct subscribe_tuple {
    neu_list_node    node;
    char *           topic;
    int              qos;
    subscribe_handle handle;
    bool             subscribed;
    mqttc_client_t * client;
};

client_error mqttc_client_subscribe_send(mqttc_client_t *        client,
                                         struct subscribe_tuple *tuple);

static void subscribe_all_topic(mqttc_client_t *client)
{
    struct subscribe_tuple *item;
    NEU_LIST_FOREACH(&client->subscribe_list, item)
    {
        mqttc_client_subscribe_send(client, item);
    }
}

static void reconnect_client(struct mqtt_client *client,
                             void **             reconnect_state_vptr)
{
    struct reconnect_state_t *reconnect_state =
        *((struct reconnect_state_t **) reconnect_state_vptr);

    if (client->error != MQTT_ERROR_INITIAL_RECONNECT) {
        close(client->socketfd);
    }

    if (client->error != MQTT_ERROR_INITIAL_RECONNECT) {
        log_error(
            "reconnect_client: called while client was in error state \"%s\"",
            mqtt_error_str(client->error));
    }

    int sockfd =
        open_nb_socket(reconnect_state->hostname, reconnect_state->port);
    if (sockfd == -1) {
        log_info("Failed to open socket: ");
    }

    mqtt_reinit(client, sockfd, reconnect_state->send_buf,
                reconnect_state->send_buf_sz, reconnect_state->recv_buf,
                reconnect_state->recv_buf_sz);

    /* Create an anonymous session */
    // const char *client_id = NULL;
    const char *client_id = reconnect_state->client_id;
    /* Ensure we have a clean session */
    uint8_t connect_flags = MQTT_CONNECT_CLEAN_SESSION;
    /* Send connection request to the broker. */
    mqtt_connect(client, client_id, NULL, NULL, 0, NULL, NULL, connect_flags,
                 400);

    /* Subscribe to the topic. */
    // mqtt_subscribe(client, reconnect_state->topic, 0);
    subscribe_all_topic(reconnect_state->client);
}

static void publish_callback(void **                       reconnect_state_vptr,
                             struct mqtt_response_publish *published)
{
    /* note that published->topic_name is NOT null-terminated (here we'll change
     * it to a c-string) */
    char *topic_name = (char *) malloc(published->topic_name_size + 1);
    memcpy(topic_name, published->topic_name, published->topic_name_size);
    topic_name[published->topic_name_size] = '\0';

    log_info("Received publish('%s'): %s", topic_name,
             (const char *) published->application_message);

    struct reconnect_state_t *reconnect_state =
        *((struct reconnect_state_t **) reconnect_state_vptr);
    mqttc_client_t *        client = reconnect_state->client;
    struct subscribe_tuple *item;
    NEU_LIST_FOREACH(&client->subscribe_list, item)
    {
        if (0 == strcmp(item->topic, topic_name) && NULL != item->handle) {
            item->handle(topic_name, strlen(topic_name),
                         (void *) published->application_message,
                         published->application_message_size,
                         client->user_data);
        }
    }

    free(topic_name);
}

static void *client_refresher(void *client)
{
    while (1) {
        mqtt_sync((struct mqtt_client *) client);
        usleep(INTERVAL);
    }
    return NULL;
}

mqttc_client_t *mqttc_client_create(option_t *option, void *context)
{
    mqttc_client_t *client = malloc(sizeof(mqttc_client_t));
    if (NULL == client) {
        log_error("Failed to malloc client memory");
        return NULL;
    }
    memset(client, 0, sizeof(mqttc_client_t));

    client->option    = option;
    client->user_data = context;

    struct reconnect_state_t *reconnect_state = &client->reconnect_state;
    reconnect_state->hostname                 = client->option->host;
    reconnect_state->port                     = client->option->port;
    reconnect_state->client_id                = client->option->clientid;
    reconnect_state->topic                    = client->option->topic;
    reconnect_state->client                   = client;
    reconnect_state->send_buf                 = client->send_buf;
    reconnect_state->send_buf_sz              = sizeof(client->send_buf);
    reconnect_state->recv_buf                 = client->recv_buf;
    reconnect_state->recv_buf_sz              = sizeof(client->recv_buf);

    client->mqttc.publish_response_callback_state = reconnect_state;

    NEU_LIST_INIT(&client->subscribe_list, struct subscribe_tuple, node);
    return client;
}

client_error mqttc_client_connect(mqttc_client_t *client)
{
    if (NULL == client) {
        log_error("The client is not initialized");
        return ClientIsNULL;
    }

    mqtt_init_reconnect(&client->mqttc, reconnect_client,
                        &client->reconnect_state, publish_callback);

    if (pthread_create(&client->client_daemon, NULL, client_refresher,
                       &client->mqttc)) {
        log_error("Failed to start client daemon.");
        return ClientConnectFailure;
    }

    log_info("Successfully start the request of connect.");
    return ClientSuccess;
}

client_error mqttc_client_open(option_t *option, void *context,
                               mqttc_client_t **p_client)
{
    mqttc_client_t *client = mqttc_client_create(option, context);
    if (NULL == client) {
        return ClientIsNULL;
    }

    client_error rc = mqttc_client_connect(client);
    *p_client       = client;
    return rc;
}

client_error mqttc_client_disconnect(mqttc_client_t *client)
{
    struct subscribe_tuple *item;
    NEU_LIST_FOREACH(&client->subscribe_list, item)
    {
        client_error error = mqttc_client_unsubscribe(client, item->topic);
        if (ClientSuccess != error) {
            // TODO: Error handle
            continue;
        }
    }

    if (NULL != client) {
        int rc = mqtt_disconnect(&client->mqttc);
        if (1 != rc) {
            // TODO: Disconnect failed
        }

        if (-1 != client->mqttc.socketfd) {
            close(client->mqttc.socketfd);
        }
    }
    return ClientSuccess;
}

client_error mqttc_client_destroy(mqttc_client_t *client)
{
    while (!neu_list_empty(&client->subscribe_list)) {
        struct subscribe_tuple *tuple = neu_list_first(&client->subscribe_list);
        neu_list_remove(&client->subscribe_list, tuple);
    }

    if (NULL != &client->client_daemon) {
        pthread_cancel(client->client_daemon);
    }

    free(client);
    return ClientSuccess;
}

struct subscribe_tuple *
mqttc_client_subscribe_create(mqttc_client_t *client, const char *topic,
                              const int qos, const subscribe_handle handle)
{
    struct subscribe_tuple *tuple =
        (struct subscribe_tuple *) malloc(sizeof(struct subscribe_tuple));

    if (NULL == tuple) {
        log_error("Failed to malloc client memory");
        return NULL;
    }

    tuple->topic      = strdup(topic);
    tuple->qos        = qos;
    tuple->handle     = handle;
    tuple->subscribed = false;
    tuple->client     = client;

    return tuple;
}

client_error mqttc_client_subscribe_destroy(struct subscribe_tuple *tuple)
{
    if (NULL != tuple) {
        if (NULL != tuple->topic) {
            free(tuple->topic);
        }
        free(tuple);
    }
    return ClientSuccess;
}

client_error mqttc_client_subscribe_send(mqttc_client_t *        client,
                                         struct subscribe_tuple *tuple)
{
    int rc = mqtt_subscribe(&client->mqttc, tuple->topic, tuple->qos);
    if (1 != rc) {
        log_error("Failed to start subscribe, return code %d", rc);
        return ClientSubscribeFailure;
    }
    return ClientSuccess;
}

client_error mqttc_client_subscribe_add(mqttc_client_t *        client,
                                        struct subscribe_tuple *tuple)
{
    NEU_LIST_NODE_INIT(&tuple->node);
    neu_list_append(&client->subscribe_list, tuple);
    return ClientSuccess;
}

client_error mqttc_client_subscribe_remove(mqttc_client_t *client,
                                           const char *    topic)
{
    struct subscribe_tuple *item;
    struct subscribe_tuple *tuple;
    NEU_LIST_FOREACH(&client->subscribe_list, item)
    {
        if (0 == strcmp(item->topic, topic)) {
            tuple = item;
        }
    }
    neu_list_remove(&client->subscribe_list, tuple);
    return ClientSuccess;
}

client_error mqttc_client_subscribe(mqttc_client_t *client, const char *topic,
                                    const int qos, subscribe_handle handle)
{
    log_info("Subscribing to topic %s for using QoS%d", topic, qos);
    struct subscribe_tuple *tuple =
        mqttc_client_subscribe_create(client, topic, qos, handle);
    if (NULL == tuple) {
        return ClientSubscribeFailure;
    }

    client_error error = mqttc_client_subscribe_add(client, tuple);
    error              = mqttc_client_subscribe_send(client, tuple);
    if (ClientSuccess != error) {
        return ClientSubscribeFailure;
    }
    return ClientSuccess;
}

struct subscribe_tuple *mqttc_client_unsubscribe_create(mqttc_client_t *client,
                                                        const char *    topic)
{
    struct subscribe_tuple *item;
    NEU_LIST_FOREACH(&client->subscribe_list, item)
    {
        if (0 == strcmp(item->topic, topic)) {
            return item;
        }
    }

    log_error("Cant find topic %s", topic);
    return NULL;
}

client_error mqttc_client_unsubscribe_send(mqttc_client_t *        client,
                                           struct subscribe_tuple *tuple)
{
    int rc = mqtt_unsubscribe(&client->mqttc, tuple->topic);
    if (1 != rc) {
        return ClientUnsubscribeFailure;
    }
    return ClientSuccess;
}

client_error mqttc_client_unsubscribe(mqttc_client_t *client, const char *topic)
{
    if (NULL == client) {
        return ClientIsNULL;
    }

    struct subscribe_tuple *tuple =
        mqttc_client_unsubscribe_create(client, topic);
    if (NULL == tuple) {
        return ClientUnsubscribeFailure;
    }

    mqttc_client_unsubscribe_send(client, tuple);

    client_error error = mqttc_client_subscribe_remove(client, topic);
    if (ClientSuccess != error) {
        return error;
    }
    return ClientSuccess;
}

client_error mqttc_client_publish(mqttc_client_t *client, const char *topic,
                                  int qos, unsigned char *payload, size_t len)
{
    log_info("Publish to topic %s for using QoS%d", topic, qos);
    int rc = mqtt_publish(&client->mqttc, topic, payload, len, qos);
    if (1 != rc) {
        log_error("Failed to start send message, return code %d", rc);
        return ClientPublishFailure;
    }
    return ClientSuccess;
}

client_error mqttc_client_close(mqttc_client_t *client)
{
    if (NULL != client) {
        return ClientIsNULL;
    }

    client_error rc = mqttc_client_disconnect(client);
    rc              = mqttc_client_destroy(client);
    return rc;
}
