#ifndef FLIGHT_SQL_CLIENT_H
#define FLIGHT_SQL_CLIENT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { INT_TYPE, FLOAT_TYPE, BOOL_TYPE, STRING_TYPE } ValueType;

typedef union {
    int         int_value;
    float       float_value;
    bool        bool_value;
    const char *string_value;
} ValueUnion;

typedef struct {
    const char *node_name;
    const char *group_name;
    const char *tag;
    ValueUnion  value;
    ValueType   value_type;
} datatag;

typedef struct {
    ValueType  value_type;
    ValueUnion value;
    char       time[64];
    char       node_name[128];
    char       group_name[128];
    char       tag[128];
} datarow;

typedef struct {
    size_t   row_count;
    datarow *rows;
} query_result;

typedef struct Client neu_datalayers_client;

neu_datalayers_client *client_create(const char *host, int port,
                                     const char *username,
                                     const char *password);

int client_execute(neu_datalayers_client *client, const char *sql);

void client_destroy(neu_datalayers_client *client);

int client_insert(neu_datalayers_client *client, ValueType type, datatag *tags,
                  size_t tag_count);

query_result *client_query(neu_datalayers_client *client, ValueType type,
                           const char *node_name, const char *group_name,
                           const char *tag);

void client_query_free(query_result *result);

query_result *client_query_nodes_groups(neu_datalayers_client *client,
                                        ValueType              type);

query_result *client_query_all_data(neu_datalayers_client *client,
                                    ValueType              type);

#ifdef __cplusplus
}
#endif

#endif