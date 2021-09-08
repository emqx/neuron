#include <cstdlib>
#include <gtest/gtest.h>

#include "parser/neu_json_group_config.h"
#include "parser/neu_json_parser.h"

TEST(TagGroupConfigTest, neu_parse_decode_get_group_config_req)
{
    char *json = (char *) "{ \"function\": 29, \"uuid\": "
                          "\"1d892fe8-f37e-11eb-9a34-932aa06a28f3\", "
                          "\"nodeId\": 123456, "
                          "\"groupConfig\": \"Config0\" }";

    void *                                 result = NULL;
    struct neu_parse_get_group_config_req *req    = NULL;

    EXPECT_EQ(0, neu_parse_decode(json, &result));

    req = (struct neu_parse_get_group_config_req *) result;
    EXPECT_EQ(NEU_PARSE_OP_GET_GROUP_CONFIG, req->function);
    EXPECT_STREQ("1d892fe8-f37e-11eb-9a34-932aa06a28f3", req->uuid);
    EXPECT_EQ(123456, req->node_id);
    EXPECT_STREQ("Config0", req->config);
    neu_parse_decode_free(result);
}

TEST(TagGroupConfigTest, neu_parse_encode_get_group_config_res)
{
    char *buf =
        (char *) "{\"function\": 29, \"uuid\": "
                 "\"1d892fe8-f37e-11eb-9a34-932aa06a28f3\", \"error\": 0, "
                 "\"groupConfigs\": [{\"name\": \"config0\", \"readInterval\": "
                 "0, \"pipeCount\": 0, \"tagCount\": 0}, {\"name\": "
                 "\"config1\", \"readInterval\": 0, \"pipeCount\": 0, "
                 "\"tagCount\": 0}, {\"name\": \"config2\", \"readInterval\": "
                 "0, \"pipeCount\": 0, \"tagCount\": 0}]}";
    char *                                result = NULL;
    struct neu_parse_get_group_config_res res    = {
        .function = NEU_PARSE_OP_GET_GROUP_CONFIG,
        .uuid     = (char *) "1d892fe8-f37e-11eb-9a34-932aa06a28f3",
        .error    = 0,
        .n_config = 3,
    };

    res.rows = (struct neu_parse_get_group_config_res_row *) calloc(
        3, sizeof(struct neu_parse_get_group_config_res_row));

    res.rows[0].name = strdup((char *) "config0");
    res.rows[1].name = strdup((char *) "config1");
    res.rows[2].name = strdup((char *) "config2");

    EXPECT_EQ(0, neu_parse_encode(&res, &result));
    EXPECT_STREQ(buf, result);

    free(res.rows[0].name);
    free(res.rows[1].name);
    free(res.rows[2].name);
    free(res.rows);
    free(result);
}

TEST(TagGroupConfigTest, neu_parse_decode_add_group_config_req)
{
    char *buf = (char *) "{\"function\":90, "
                         "\"uuid\":\"554f5fd8-f437-11eb-975c-7704b9e17821\", "
                         "\"groupConfig\":\"Config0\", "
                         "\"srcNodeId\":123456, "
                         "\"dstNodeId\":654321, "
                         "\"readInterval\": 2000 "
                         " }";

    void *                                 result = NULL;
    struct neu_parse_add_group_config_req *req    = NULL;

    EXPECT_EQ(0, neu_parse_decode(buf, &result));

    req = (struct neu_parse_add_group_config_req *) result;

    EXPECT_EQ(NEU_PARSE_OP_ADD_GROUP_CONFIG, req->function);
    EXPECT_STREQ("554f5fd8-f437-11eb-975c-7704b9e17821", req->uuid);
    EXPECT_STREQ("Config0", req->config);
    EXPECT_EQ(123456, req->src_node_id);
    EXPECT_EQ(654321, req->dst_node_id);
    EXPECT_EQ(2000, req->read_interval);

    neu_parse_decode_free(result);
}

TEST(TagGroupConfigTest, neu_parse_encode_add_group_config_res)
{
    char *buf = (char *) "{\"function\": 90, "
                         "\"uuid\": \"554f5fd8-f437-11eb-975c-7704b9e17821\", "
                         "\"error\": 0}";
    char *                            result = NULL;
    struct neu_parse_group_config_res res    = {
        .function = NEU_PARSE_OP_ADD_GROUP_CONFIG,
        .uuid     = (char *) "554f5fd8-f437-11eb-975c-7704b9e17821",
        .error    = 0,
    };

    EXPECT_EQ(0, neu_parse_encode(&res, &result));
    EXPECT_STREQ(buf, result);

    free(result);
}

TEST(TagGroupConfigTest, neu_parse_decode_update_group_config_req)
{
    char *buf = (char *) "{\"function\":91, "
                         "\"uuid\":\"554f5fd8-f437-11eb-975c-7704b9e17821\", "
                         "\"groupConfig\":\"Config0\", "
                         "\"srcNodeId\":123456, "
                         "\"dstNodeId\":654321, "
                         "\"readInterval\": 2000 "
                         " }";

    void *                                    result = NULL;
    struct neu_parse_update_group_config_req *req    = NULL;

    EXPECT_EQ(0, neu_parse_decode(buf, &result));

    req = (struct neu_parse_update_group_config_req *) result;

    EXPECT_EQ(NEU_PARSE_OP_UPDATE_GROUP_CONFIG, req->function);
    EXPECT_STREQ("554f5fd8-f437-11eb-975c-7704b9e17821", req->uuid);
    EXPECT_STREQ("Config0", req->config);
    EXPECT_EQ(123456, req->src_node_id);
    EXPECT_EQ(654321, req->dst_node_id);
    EXPECT_EQ(2000, req->read_interval);

    neu_parse_decode_free(result);
}

TEST(TagGroupConfigTest, neu_parse_encode_update_group_config_res)
{
    char *buf = (char *) "{\"function\": 91, "
                         "\"uuid\": \"554f5fd8-f437-11eb-975c-7704b9e17821\", "
                         "\"error\": 0}";
    char *                            result = NULL;
    struct neu_parse_group_config_res res    = {
        .function = NEU_PARSE_OP_UPDATE_GROUP_CONFIG,
        .uuid     = (char *) "554f5fd8-f437-11eb-975c-7704b9e17821",
        .error    = 0,
    };

    EXPECT_EQ(0, neu_parse_encode(&res, &result));
    EXPECT_STREQ(buf, result);

    free(result);
}

TEST(TagGroupConfigTest, neu_parse_decode_delete_group_config_req)
{
    char *buf = (char *) "{\"function\":92, "
                         "\"uuid\":\"554f5fd8-f437-11eb-975c-7704b9e17821\", "
                         "\"groupConfig\":\"Config0\", "
                         "\"nodeId\":123456 "
                         "}";

    void *                                    result = NULL;
    struct neu_parse_delete_group_config_req *req    = NULL;

    EXPECT_EQ(0, neu_parse_decode(buf, &result));

    req = (struct neu_parse_delete_group_config_req *) result;

    EXPECT_EQ(NEU_PARSE_OP_DELETE_GROUP_CONFIG, req->function);
    EXPECT_STREQ("554f5fd8-f437-11eb-975c-7704b9e17821", req->uuid);
    EXPECT_STREQ("Config0", req->config);
    EXPECT_EQ(123456, req->node_id);

    neu_parse_decode_free(result);
}

TEST(TagGroupConfigTest, neu_parse_encode_delete_group_config_res)
{
    char *buf = (char *) "{\"function\": 92, "
                         "\"uuid\": \"554f5fd8-f437-11eb-975c-7704b9e17821\", "
                         "\"error\": 0}";
    char *                            result = NULL;
    struct neu_parse_group_config_res res    = {
        .function = NEU_PARSE_OP_DELETE_GROUP_CONFIG,
        .uuid     = (char *) "554f5fd8-f437-11eb-975c-7704b9e17821",
        .error    = 0,
    };

    EXPECT_EQ(0, neu_parse_encode(&res, &result));
    EXPECT_STREQ(buf, result);

    free(result);
}

TEST(TagGroupConfigTest, neu_parse_decode_add_tag_ids_req)
{
    char *buf = (char *) "{\"function\":93,\"uuid\":\"554f5fd8-f437-11eb-975c-"
                         "7704b9e17821\",\"groupConfig\":"
                         "\"Config0\",\"nodeId\":123456,\"dataTagIds\":[{"
                         "\"dataTagName\":\"123\",\"dataTagId\":123},{"
                         "\"dataTagName\":\"321\",\"dataTagId\":321}]}";

    void *                            result = NULL;
    struct neu_parse_add_tag_ids_req *req    = NULL;

    EXPECT_EQ(0, neu_parse_decode(buf, &result));

    req = (struct neu_parse_add_tag_ids_req *) result;

    EXPECT_EQ(NEU_PARSE_OP_ADD_DATATAG_IDS_CONFIG, req->function);
    EXPECT_STREQ("554f5fd8-f437-11eb-975c-7704b9e17821", req->uuid);
    EXPECT_STREQ("Config0", req->config);
    EXPECT_EQ(123456, req->node_id);

    EXPECT_STREQ("123", req->rows[0].datatag_name);
    EXPECT_EQ(123, req->rows[0].datatag_id);

    EXPECT_STREQ("321", req->rows[1].datatag_name);
    EXPECT_EQ(321, req->rows[1].datatag_id);

    neu_parse_decode_free(result);
}

TEST(TagGroupConfigTest, neu_parse_encode_add_tag_ids_res)
{
    char *buf = (char *) "{\"function\": 93, \"uuid\": "
                         "\"554f5fd8-f437-11eb-975c-7704b9e17821\", "
                         "\"error\": 0}";

    char *                           result = NULL;
    struct neu_parse_add_tag_ids_res res    = {
        .function = NEU_PARSE_OP_ADD_DATATAG_IDS_CONFIG,
        .uuid     = (char *) "554f5fd8-f437-11eb-975c-7704b9e17821",
    };

    EXPECT_EQ(0, neu_parse_encode(&res, &result));
    EXPECT_STREQ(buf, result);

    free(result);
}

TEST(TagGroupConfigTest, neu_paser_decode_delete_tag_ids_req)
{
    char *buf = (char *) "{\"function\":94,\"uuid\":\"554f5fd8-f437-11eb-975c-"
                         "7704b9e17821\",\"groupConfig\":"
                         "\"Config0\",\"nodeId\":123456,\"dataTagIds\":[{"
                         "\"dataTagName\":\"123\",\"dataTagId\":123},{"
                         "\"dataTagName\":\"321\",\"dataTagId\":321}]}";

    void *                               result = NULL;
    struct neu_parse_delete_tag_ids_req *req    = NULL;

    EXPECT_EQ(0, neu_parse_decode(buf, &result));

    req = (struct neu_parse_delete_tag_ids_req *) result;

    EXPECT_EQ(NEU_PARSE_OP_DELETE_DATATAG_IDS_CONFIG, req->function);
    EXPECT_STREQ("554f5fd8-f437-11eb-975c-7704b9e17821", req->uuid);
    EXPECT_STREQ("Config0", req->config);
    EXPECT_EQ(123456, req->node_id);

    EXPECT_STREQ("123", req->rows[0].datatag_name);
    EXPECT_EQ(123, req->rows[0].datatag_id);

    EXPECT_STREQ("321", req->rows[1].datatag_name);
    EXPECT_EQ(321, req->rows[1].datatag_id);

    neu_parse_decode_free(result);
}

TEST(TagGroupConfigTest, neu_parse_encode_delete_tag_ids_res)
{
    char *buf = (char *) "{\"function\": 94, \"uuid\": "
                         "\"554f5fd8-f437-11eb-975c-7704b9e17821\", "
                         "\"error\": 0}";

    char *                              result = NULL;
    struct neu_parse_delete_tag_ids_res res    = {
        .function = NEU_PARSE_OP_DELETE_DATATAG_IDS_CONFIG,
        .uuid     = (char *) "554f5fd8-f437-11eb-975c-7704b9e17821",
    };

    EXPECT_EQ(0, neu_parse_encode(&res, &result));
    EXPECT_STREQ(buf, result);

    free(result);
}