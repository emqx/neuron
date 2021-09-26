#include <cstdlib>
#include <gtest/gtest.h>

#include "parser/neu_json_fn.h"
#include "parser/neu_json_group_config.h"

TEST(TagGroupConfigTest, neu_parse_decode_get_group_config_req)
{
    char *json = (char *) "{\"node_id\": 123456, "
                          "\"name\": \"Config0\" }";

    neu_parse_get_group_config_req_t *req = NULL;

    EXPECT_EQ(0, neu_parse_decode_get_group_config(json, &req));

    EXPECT_EQ(123456, req->node_id);
    EXPECT_STREQ("Config0", req->name);

    neu_parse_decode_get_group_config_free(req);
}

TEST(TagGroupConfigTest, neu_parse_encode_get_group_config_res)
{
    char *buf =
        (char *) "{\"group_configs\": [{\"name\": \"config0\", \"interval\": "
                 "0, \"pipe_count\": 0, \"tag_count\": 0}, {\"name\": "
                 "\"config1\", \"interval\": 0, \"pipe_count\": 0, "
                 "\"tag_count\": 0}, {\"name\": \"config2\", \"interval\": 0, "
                 "\"pipe_count\": 0, \"tag_count\": 0}]}";
    char *                           result = NULL;
    neu_parse_get_group_config_res_t res    = {
        .n_config = 3,
    };

    res.rows = (neu_parse_get_group_config_res_row_t *) calloc(
        3, sizeof(neu_parse_get_group_config_res_row_t));

    res.rows[0].name = strdup((char *) "config0");
    res.rows[1].name = strdup((char *) "config1");
    res.rows[2].name = strdup((char *) "config2");

    EXPECT_EQ(0,
              neu_json_encode_by_fn(&res, neu_parse_encode_get_group_config,
                                    &result));
    EXPECT_STREQ(buf, result);

    free(res.rows[0].name);
    free(res.rows[1].name);
    free(res.rows[2].name);
    free(res.rows);
    free(result);
}

TEST(TagGroupConfigTest, neu_parse_decode_add_group_config_req)
{
    char *buf = (char *) "{\"name\":\"Config0\", "
                         "\"src_node_id\":123456, "
                         "\"dst_node_id\":654321, "
                         "\"interval\": 2000 "
                         " }";

    neu_parse_add_group_config_req_t *req = NULL;

    EXPECT_EQ(0, neu_parse_decode_add_group_config(buf, &req));

    EXPECT_STREQ("Config0", req->name);
    EXPECT_EQ(123456, req->src_node_id);
    EXPECT_EQ(654321, req->dst_node_id);
    EXPECT_EQ(2000, req->interval);

    neu_parse_decode_add_group_config_free(req);
}

TEST(TagGroupConfigTest, neu_parse_decode_update_group_config_req)
{
    char *buf = (char *) "{\"name\":\"Config0\", "
                         "\"src_node_id\":123456, "
                         "\"dst_node_id\":654321, "
                         "\"interval\": 2000 "
                         " }";

    neu_parse_update_group_config_req_t *req = NULL;

    EXPECT_EQ(0, neu_parse_decode_update_group_config(buf, &req));

    EXPECT_STREQ("Config0", req->name);
    EXPECT_EQ(123456, req->src_node_id);
    EXPECT_EQ(654321, req->dst_node_id);
    EXPECT_EQ(2000, req->interval);

    neu_parse_decode_update_group_config_free(req);
}

TEST(TagGroupConfigTest, neu_parse_decode_delete_group_config_req)
{
    char *buf = (char *) "{\"name\":\"Config0\", "
                         "\"node_id\":123456 "
                         "}";

    neu_parse_del_group_config_req_t *req = NULL;

    EXPECT_EQ(0, neu_parse_decode_del_group_config(buf, &req));

    EXPECT_STREQ("Config0", req->name);
    EXPECT_EQ(123456, req->node_id);

    neu_parse_decode_del_group_config_free(req);
}
