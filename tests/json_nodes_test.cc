#include <stdlib.h>

#include <gtest/gtest.h>

#include "adapter.h"
#include "json/neu_json_fn.h"
#include "json/neu_json_node.h"

TEST(JsonNodesTest, AddNodesDecode)
{
    char *buf = (char *) "{\"type\": 1, "
                         "\"name\": \"adapter1\" ,"
                         "\"plugin_name\": \"plugin1\" }";
    neu_json_add_node_req_t *req = NULL;

    EXPECT_EQ(0, neu_json_decode_add_node_req(buf, &req));

    EXPECT_EQ(NEU_NODE_TYPE_DRIVER, req->type);
    EXPECT_STREQ("adapter1", req->name);
    EXPECT_STREQ("plugin1", req->plugin_name);

    neu_json_decode_add_node_req_free(req);
}

TEST(JsonNodesTest, GetNodesDecode)
{
    char *                    buf = (char *) "{\"node_type\": 1 }";
    neu_json_get_nodes_req_t *req = NULL;

    EXPECT_EQ(0, neu_json_decode_get_nodes_req(buf, &req));

    EXPECT_EQ(1, req->node_type);

    neu_json_decode_get_nodes_req_free(req);
}

TEST(JsonNodesTest, GetNodesEncode)
{
    char *buf =
        (char *) "{\"nodes\": [{\"plugin_id\": 0, \"name\": \"node1\", \"id\": "
                 "123}, {\"plugin_id\": 0, \"name\": \"node2\", \"id\": 456}, "
                 "{\"plugin_id\": 0, \"name\": \"node3\", \"id\": 789}]}";
    char *                    result = NULL;
    neu_json_get_nodes_resp_t res    = {
        .n_node = 3,
    };

    res.nodes = (neu_json_get_nodes_resp_node_t *) calloc(
        3, sizeof(neu_json_get_nodes_resp_node_t));
    res.nodes[0].name = strdup((char *) "node1");
    res.nodes[0].id   = 123;

    res.nodes[1].name = strdup((char *) "node2");
    res.nodes[1].id   = 456;

    res.nodes[2].name = strdup((char *) "node3");
    res.nodes[2].id   = 789;

    EXPECT_EQ(
        0,
        neu_json_encode_by_fn(&res, neu_json_encode_get_nodes_resp, &result));
    EXPECT_STREQ(buf, result);

    free(res.nodes[0].name);
    free(res.nodes[1].name);
    free(res.nodes[2].name);
    free(res.nodes);

    free(result);
}

TEST(JsonNodesTest, DeleteNodesDecode)
{
    char *                   buf = (char *) "{\"id\": 123 }";
    neu_json_del_node_req_t *req = NULL;

    EXPECT_EQ(0, neu_json_decode_del_node_req(buf, &req));

    EXPECT_EQ(123, req->id);

    neu_json_decode_del_node_req_free(req);
}

TEST(JsonNodesTest, UpdateNodesDecode)
{
    char *buf = (char *) "{\"id\": 1, "
                         "\"name\": \"adapter1\" }";
    neu_json_update_node_req_t *req = NULL;

    EXPECT_EQ(0, neu_json_decode_update_node_req(buf, &req));

    EXPECT_EQ(1, req->id);
    EXPECT_STREQ("adapter1", req->name);

    neu_json_decode_update_node_req_free(req);
}
