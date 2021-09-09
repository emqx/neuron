#include <stdlib.h>

#include <gtest/gtest.h>

#include "parser/neu_json_node.h"
#include "parser/neu_json_parser.h"

TEST(JsonNodesTest, AddNodesDecode)
{
    char *buf = (char *) "{\"function\": 35, "
                         "\"uuid\": \"554f5fd8-f437-11eb-975c-7704b9e17821\", "
                         "\"node_type\": 1, "
                         "\"adapter_name\": \"adapter1\" ,"
                         "\"plugin_name\": \"plugin1\" }";
    void *                          result = NULL;
    struct neu_parse_add_nodes_req *req    = NULL;

    EXPECT_EQ(0, neu_parse_decode(buf, &result));

    req = (struct neu_parse_add_nodes_req *) result;

    EXPECT_EQ(NEU_PARSE_OP_ADD_NODES, req->function);
    EXPECT_STREQ("554f5fd8-f437-11eb-975c-7704b9e17821", req->uuid);
    EXPECT_EQ(NEU_NODE_TYPE_DRIVER, req->node_type);
    EXPECT_STREQ("adapter1", req->adapter_name);
    EXPECT_STREQ("plugin1", req->plugin_name);

    neu_parse_decode_free(result);
}

TEST(JsonNodesTest, AddNodesEncode)
{
    char *buf = (char *) "{\"function\": 35, "
                         "\"uuid\": \"554f5fd8-f437-11eb-975c-7704b9e17821\", "
                         "\"error\": 0}";
    char *                         result = NULL;
    struct neu_parse_add_nodes_res res    = {
        .function = NEU_PARSE_OP_ADD_NODES,
        .uuid     = (char *) "554f5fd8-f437-11eb-975c-7704b9e17821",
        .error    = 0,
    };

    EXPECT_EQ(0, neu_parse_encode(&res, &result));
    EXPECT_STREQ(buf, result);

    free(result);
}

TEST(JsonNodesTest, GetNodesDecode)
{
    char *buf = (char *) "{\"function\": 36, "
                         "\"uuid\": \"554f5fd8-f437-11eb-975c-7704b9e17821\","
                         "\"node_type\": 1 }";
    void *                          result = NULL;
    struct neu_parse_get_nodes_req *req    = NULL;

    EXPECT_EQ(0, neu_parse_decode(buf, &result));

    req = (struct neu_parse_get_nodes_req *) result;

    EXPECT_EQ(NEU_PARSE_OP_GET_NODES, req->function);
    EXPECT_STREQ("554f5fd8-f437-11eb-975c-7704b9e17821", req->uuid);
    EXPECT_EQ(1, req->node_type);

    neu_parse_decode_free(result);
}

TEST(JsonNodesTest, GetNodesEncode)
{
    char *buf = (char *) "{\"function\": 36, "
                         "\"uuid\": \"554f5fd8-f437-11eb-975c-7704b9e17821\", "
                         "\"nodes\": "
                         "[{\"name\": \"node1\", \"id\": 123}, "
                         "{\"name\": \"node2\", \"id\": 456}, "
                         "{\"name\": \"node3\", \"id\": 789}]}";
    char *                         result = NULL;
    struct neu_parse_get_nodes_res res    = {
        .function = NEU_PARSE_OP_GET_NODES,
        .uuid     = (char *) "554f5fd8-f437-11eb-975c-7704b9e17821",
        .n_node   = 3,
    };

    res.nodes = (struct neu_parse_get_nodes_res_nodes *) calloc(
        3, sizeof(struct neu_parse_get_nodes_res_nodes));
    res.nodes[0].name = strdup((char *) "node1");
    res.nodes[0].id   = 123;

    res.nodes[1].name = strdup((char *) "node2");
    res.nodes[1].id   = 456;

    res.nodes[2].name = strdup((char *) "node3");
    res.nodes[2].id   = 789;

    EXPECT_EQ(0, neu_parse_encode(&res, &result));
    EXPECT_STREQ(buf, result);

    free(res.nodes[0].name);
    free(res.nodes[1].name);
    free(res.nodes[2].name);
    free(res.nodes);

    free(result);
}

TEST(JsonNodesTest, DeleteNodesDecode)
{
    char *buf = (char *) "{\"function\": 37, "
                         "\"uuid\": \"554f5fd8-f437-11eb-975c-7704b9e17821\","
                         "\"node_id\": 123 }";
    void *                             result = NULL;
    struct neu_parse_delete_nodes_req *req    = NULL;

    EXPECT_EQ(0, neu_parse_decode(buf, &result));

    req = (struct neu_parse_delete_nodes_req *) result;

    EXPECT_EQ(NEU_PARSE_OP_DELETE_NODES, req->function);
    EXPECT_STREQ("554f5fd8-f437-11eb-975c-7704b9e17821", req->uuid);
    EXPECT_EQ(123, req->node_id);

    neu_parse_decode_free(result);
}

TEST(JsonNodesTest, DeleteNodesEncode)
{
    char *buf = (char *) "{\"function\": 37, "
                         "\"uuid\": \"554f5fd8-f437-11eb-975c-7704b9e17821\", "
                         "\"error\": 0}";
    char *                            result = NULL;
    struct neu_parse_delete_nodes_res res    = {
        .function = NEU_PARSE_OP_DELETE_NODES,
        .uuid     = (char *) "554f5fd8-f437-11eb-975c-7704b9e17821",
        .error    = 0,
    };

    EXPECT_EQ(0, neu_parse_encode(&res, &result));
    EXPECT_STREQ(buf, result);

    free(result);
}

TEST(JsonNodesTest, UpdateNodesDecode)
{
    char *buf = (char *) "{\"function\": 38, "
                         "\"uuid\": \"554f5fd8-f437-11eb-975c-7704b9e17821\", "
                         "\"node_type\": 1, "
                         "\"adapter_name\": \"adapter1\", "
                         "\"plugin_name\":  \"plugin1\" }";
    void *                             result = NULL;
    struct neu_parse_update_nodes_req *req    = NULL;

    EXPECT_EQ(0, neu_parse_decode(buf, &result));

    req = (struct neu_parse_update_nodes_req *) result;

    EXPECT_EQ(NEU_PARSE_OP_UPDATE_NODES, req->function);
    EXPECT_STREQ("554f5fd8-f437-11eb-975c-7704b9e17821", req->uuid);
    EXPECT_EQ(1, req->node_type);
    EXPECT_STREQ("adapter1", req->adapter_name);
    EXPECT_STREQ("plugin1", req->plugin_name);

    neu_parse_decode_free(result);
}

TEST(JsonNodesTest, UpdaeNodesEncode)
{
    char *buf = (char *) "{\"function\": 38, "
                         "\"uuid\": \"554f5fd8-f437-11eb-975c-7704b9e17821\", "
                         "\"error\": 0}";
    char *                            result = NULL;
    struct neu_parse_update_nodes_res res    = {
        .function = NEU_PARSE_OP_UPDATE_NODES,
        .uuid     = (char *) "554f5fd8-f437-11eb-975c-7704b9e17821",
        .error    = 0,
    };

    EXPECT_EQ(0, neu_parse_encode(&res, &result));
    EXPECT_STREQ(buf, result);

    free(result);
}