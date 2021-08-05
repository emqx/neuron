#ifndef NEURON_PLUGIN_PAHO_OPTION
#define NEURON_PLUGIN_PAHO_OPTION

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /* debug app options */
    int   publisher; /* publisher app? */
    int   quiet;
    int   verbose;
    int   tracelevel;
    char *delimiter;
    int   maxdatalen;
    /* message options */
    char *message;
    char *filename;
    int   stdin_lines;
    int   stdlin_complete;
    int   null_message;
    /* MQTT options */
    int   MQTT_version; // 3-3_1, 4-3_3_1, 5-5
    char *topic;
    char *clientid;
    int   qos;
    int   retained;
    char *username;
    char *password;
    char *host;
    char *port;
    char *connection;
    int   keepalive;

    int keepalive_interval;
    int clean_session;
    /* will options */
    char *will_topic;
    char *will_payload;
    int   will_qos;
    int   will_retain;
    /* TLS options */
    int   insecure;
    char *capath;
    char *cert;
    char *cafile;
    char *key;
    char *keypass;
    char *ciphers;
    char *psk_identity;
    char *psk;
    /* MQTT V5 options */
    int message_expiry;
    struct {
        char *name;
        char *value;
    } user_property;
    /* websocket HTTP proxies */
    char *http_proxy;
    char *https_proxy;
} option_t;

#ifdef __cplusplus
}
#endif
#endif