#include <stdlib.h>

#include <gtest/gtest.h>

#include "json/neu_json_fn.h"
#include "json/neu_json_tag.h"

TEST(JsonDatatagTableTest, AddTagDecode)
{
    char *buf = (char *) "{\"node_id\":123456, \"group_config_name\": \"name\","
                         "\"tags\": "
                         "[{\"name\":\"name1\", \"attribute\":1, "
                         "\"address\":\"1!400001\", \"type\":0}, "
                         "{\"name\":\"name2\", \"attribute\":3,"
                         "\"address\":\"1!400002\", \"type\":1} ] }";
    neu_json_add_tags_req_t *req = NULL;

    EXPECT_EQ(0, neu_json_decode_add_tags_req(buf, &req));

    EXPECT_EQ(123456, req->node_id);
    EXPECT_STREQ("name", req->group_config_name);

    EXPECT_STREQ("name1", req->tags[0].name);
    EXPECT_EQ(0, req->tags[0].type);
    EXPECT_STREQ("1!400001", req->tags[0].address);
    EXPECT_EQ(1, req->tags[0].attribute);

    EXPECT_STREQ("name2", req->tags[1].name);
    EXPECT_EQ(1, req->tags[1].type);
    EXPECT_STREQ("1!400002", req->tags[1].address);
    EXPECT_EQ(3, req->tags[1].attribute);

    neu_json_decode_add_tags_req_free(req);
}

TEST(JsonDatatagTableTest, GetTagDecode)
{
    char *buf = (char *) "{\"node_id\": 123456, \"group_config_name\": "
                         "\"config_modbus_tcp_sample_2\"}";
    neu_json_get_tags_req_t *req = NULL;

    EXPECT_EQ(0, neu_json_decode_get_tags_req(buf, &req));

    EXPECT_EQ(123456, req->node_id);

    neu_json_decode_get_tags_req_free(req);
}

TEST(JsonDatatagTableTest, GetTagEncode)
{
    char *buf =
        (char *) "{\"tags\": [{\"type\": 0, \"name\": \"Tag0001\", \"id\": 0, "
                 "\"group_config_name\": \"config_modbus_tcp_sample_2\", "
                 "\"attribute\": 0, \"address\": \"1!400001\"}, {\"type\": 1, "
                 "\"name\": \"Tag0002\", \"id\": 1, \"group_config_name\": "
                 "\"config_modbus_tcp_sample_2\", \"attribute\": 1, "
                 "\"address\": \"1!400002\"}]}";
    char *                   result = NULL;
    neu_json_get_tags_resp_t res    = { 0 };

    res.n_tag = 2;

    res.tags = (neu_json_get_tags_resp_tag_t *) calloc(
        2, sizeof(neu_json_get_tags_resp_tag_t));
    res.tags[0].type      = 0;
    res.tags[0].name      = strdup((char *) "Tag0001");
    res.tags[0].id        = 0;
    res.tags[0].attribute = 0;
    res.tags[0].address   = strdup((char *) "1!400001");
    res.tags[0].group_config_name =
        strdup((char *) "config_modbus_tcp_sample_2");

    res.tags[1].type      = 1;
    res.tags[1].name      = strdup((char *) "Tag0002");
    res.tags[1].id        = 1;
    res.tags[1].attribute = 1;
    res.tags[1].address   = strdup((char *) "1!400002");
    res.tags[1].group_config_name =
        strdup((char *) "config_modbus_tcp_sample_2");

    EXPECT_EQ(
        0, neu_json_encode_by_fn(&res, neu_json_encode_get_tags_resp, &result));
    EXPECT_STREQ(buf, result);

    free(res.tags[0].name);
    free(res.tags[1].name);
    free(res.tags[0].address);
    free(res.tags[1].address);
    free(res.tags[0].group_config_name);
    free(res.tags[1].group_config_name);
    free(res.tags);
    free(result);
}

TEST(JsonDatatagTableTest, DeleteTagDecode)
{
    char *buf =
        (char *) "{\"node_id\": 123456, \"group_config_name\": \"name\", "
                 "\"ids\": [123, 456]}";
    neu_json_del_tags_req_t *req = NULL;

    EXPECT_EQ(0, neu_json_decode_del_tags_req(buf, &req));

    EXPECT_EQ(123456, req->node_id);
    EXPECT_STREQ("name", req->group_config_name);
    EXPECT_EQ(2, req->n_id);
    EXPECT_EQ(123, req->ids[0]);
    EXPECT_EQ(456, req->ids[1]);

    neu_json_decode_del_tags_req_free(req);
}

TEST(JsonDatatagTableTest, UpdateTagDecode)
{
    char *buf = (char *) "{\"node_id\":123456, "
                         "\"group_config_name\": \"test-group\","
                         "\"tags\": "
                         "[{\"id\":12,\"name\":\"name1\",\"type\":0,"
                         "\"address\":\"1!400001\",\"attribute\":0}, "
                         "{\"id\":13,\"name\":\"name2\",\"type\":1, "
                         "\"address\":\"1!400003\",\"attribute\":1} ] }";
    neu_json_update_tags_req_t *req = NULL;

    EXPECT_EQ(0, neu_json_decode_update_tags_req(buf, &req));

    EXPECT_EQ(123456, req->node_id);
    EXPECT_STREQ("test-group", req->group_config_name);

    EXPECT_EQ(12, req->tags[0].id);
    EXPECT_STREQ("name1", req->tags[0].name);
    EXPECT_EQ(0, req->tags[0].type);
    EXPECT_STREQ("1!400001", req->tags[0].address);
    EXPECT_EQ(0, req->tags[0].attribute);

    EXPECT_EQ(13, req->tags[1].id);
    EXPECT_STREQ("name2", req->tags[1].name);
    EXPECT_EQ(1, req->tags[1].type);
    EXPECT_STREQ("1!400003", req->tags[1].address);
    EXPECT_EQ(1, req->tags[1].attribute);

    neu_json_decode_update_tags_req_free(req);
}
