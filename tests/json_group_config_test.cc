#include <cstdlib>
#include <gtest/gtest.h>

#include "json/neu_json_fn.h"
#include "json/neu_json_group_config.h"

TEST(TagGroupConfigTest, neu_parse_decode_get_group_config_req)
{
    char *json = (char *) "{\"node_id\": 123456 }";

    neu_json_get_group_config_req_t *req = NULL;

    EXPECT_EQ(0, neu_json_decode_get_group_config_req(json, &req));

    EXPECT_EQ(123456, req->node_id);

    neu_json_decode_get_group_config_req_free(req);
}

TEST(TagGroupConfigTest, neu_parse_encode_get_group_config_res)
{
    char *buf =
        (char *) "{\"group_configs\": [{\"tag_count\": 0, \"pipe_count\": 0, "
                 "\"name\": \"config0\", \"interval\": 0}, {\"tag_count\": 0, "
                 "\"pipe_count\": 0, \"name\": \"config1\", \"interval\": 0}, "
                 "{\"tag_count\": 0, \"pipe_count\": 0, \"name\": \"config2\", "
                 "\"interval\": 0}]}";
    char *                           result = NULL;
    neu_json_get_group_config_resp_t res    = {
        .n_group_config = 3,
    };

    res.group_configs =
        (neu_json_get_group_config_resp_group_config_t *) calloc(
            3, sizeof(neu_json_get_group_config_resp_group_config_t));

    res.group_configs[0].name = strdup((char *) "config0");
    res.group_configs[1].name = strdup((char *) "config1");
    res.group_configs[2].name = strdup((char *) "config2");

    EXPECT_EQ(0,
              neu_json_encode_by_fn(&res, neu_json_encode_get_group_config_resp,
                                    &result));
    EXPECT_STREQ(buf, result);

    free(res.group_configs[0].name);
    free(res.group_configs[1].name);
    free(res.group_configs[2].name);
    free(res.group_configs);
    free(result);
}

TEST(TagGroupConfigTest, neu_parse_decode_add_group_config_req)
{
    char *buf = (char *) "{\"name\":\"Config0\", "
                         "\"node_id\":123456, "
                         "\"interval\": 2000 "
                         " }";

    neu_json_add_group_config_req_t *req = NULL;

    EXPECT_EQ(0, neu_json_decode_add_group_config_req(buf, &req));

    EXPECT_STREQ("Config0", req->name);
    EXPECT_EQ(123456, req->node_id);
    EXPECT_EQ(2000, req->interval);

    neu_json_decode_add_group_config_req_free(req);
}

TEST(TagGroupConfigTest, neu_parse_decode_update_group_config_req)
{
    char *buf = (char *) "{\"name\":\"Config0\", "
                         "\"node_id\":123456, "
                         "\"interval\": 2000 "
                         " }";

    neu_json_update_group_config_req_t *req = NULL;

    EXPECT_EQ(0, neu_json_decode_update_group_config_req(buf, &req));

    EXPECT_STREQ("Config0", req->name);
    EXPECT_EQ(123456, req->node_id);
    EXPECT_EQ(2000, req->interval);

    neu_json_decode_update_group_config_req_free(req);
}

TEST(TagGroupConfigTest, neu_parse_decode_delete_group_config_req)
{
    char *buf = (char *) "{\"name\":\"Config0\", "
                         "\"node_id\":123456 "
                         "}";

    neu_json_del_group_config_req_t *req = NULL;

    EXPECT_EQ(0, neu_json_decode_del_group_config_req(buf, &req));

    EXPECT_STREQ("Config0", req->name);
    EXPECT_EQ(123456, req->node_id);

    neu_json_decode_del_group_config_req_free(req);
}
