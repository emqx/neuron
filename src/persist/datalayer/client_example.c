#include "flight_sql_client.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main()
{
    Client *client = client_create("127.0.0.1", 8360, "admin", "public");

    datatag tags[] = {
        { "node_1", "group_1", "tag_1", { .string_value = "abc" }, STRING_TYPE }
    };
    size_t tag_count = sizeof(tags) / sizeof(tags[0]);

    int result_w = client_insert(client, STRING_TYPE, tags, tag_count);
    if (result_w == 0) {
        printf("Insert successful\n");
    } else {
        printf("Insert failed\n");
    }

    query_result *result =
        client_query(client, STRING_TYPE, "node_1", "group_1", "tag_1");
    if (result) {
        printf("Query returned %zu rows\n", result->row_count);
        for (size_t i = 0; i < result->row_count; ++i) {
            printf("Time: %s, Node: %s, Group: %s, Tag: %s, Value: ",
                   result->rows[i].time, result->rows[i].node_name,
                   result->rows[i].group_name, result->rows[i].tag);

            switch (result->rows[i].value_type) {
            case INT_TYPE:
                printf("%d\n", result->rows[i].value.int_value);
                break;
            case FLOAT_TYPE:
                printf("%.6f\n", result->rows[i].value.float_value);
                break;
            case BOOL_TYPE:
                printf("%s\n",
                       result->rows[i].value.bool_value ? "true" : "false");
                break;
            case STRING_TYPE:
                if (result->rows[i].value.string_value) {
                    printf("%s\n", result->rows[i].value.string_value);
                } else {
                    printf("(null)\n");
                }
                break;
            default:
                printf("Unknown type\n");
                break;
            }
        }
        client_query_free(result);
    } else {
        printf("No data found\n");
    }

    client_destroy(client);

    return 0;
}