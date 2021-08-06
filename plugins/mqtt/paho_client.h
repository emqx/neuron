#ifndef NEURON_PLUGIN_PAHO_CLIENT
#define NEURON_PLUGIN_PAHO_CLIENT

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

typedef struct paho_client paho_client_t;

typedef enum {
    ClientSuccess = 0,
    ClientIsNULL,
    ClientConnectTimeout,
    ClientSubscribeListInitialFailure,
    ClientSubscribeFailure,
    ClientPublishFailure,
} client_error;

typedef void (*subscribe_handle)(const char *topic_name, void *context,
                                 void *payload, const size_t len);

client_error paho_client_open(option_t *option, paho_client_t **client);

client_error paho_client_subscribe(paho_client_t *client, const char *topic,
                                   const int qos, subscribe_handle handle,
                                   void *context);

client_error paho_client_unsubscribe(paho_client_t *client, const char *topic);

client_error paho_client_publish(paho_client_t *client, const char *topic,
                                 int qos, unsigned char *payload, size_t len);

client_error paho_client_close(paho_client_t *client);

#ifdef __cplusplus
}
#endif
#endif