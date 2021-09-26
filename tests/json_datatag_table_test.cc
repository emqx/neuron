#include <stdlib.h>

#include <gtest/gtest.h>

#include "parser/neu_json_fn.h"
#include "parser/neu_json_tag.h"

TEST(JsonDatatagTableTest, AddTagDecode)
{
    char *buf = (char *) "{\"node_id\":123456, \"group_config_name\": \"name\","
                         "\"tags\": "
                         "[{\"name\":\"name1\", \"type\":0,\"decimal\":1, "
                         "\"address\":\"1!400001\",\"flag\":0}, "
                         "{\"name\":\"name2\", \"type\":1,\"decimal\":1, "
                         "\"address\":\"1!400002\",\"flag\":0} ] }";
    neu_parse_add_tags_req_t *req = NULL;

    EXPECT_EQ(0, neu_parse_decode_add_tags(buf, &req));

    EXPECT_EQ(123456, req->node_id);
    EXPECT_STREQ("name", req->group_config_name);

    EXPECT_STREQ("name1", req->tags[0].name);
    EXPECT_EQ(0, req->tags[0].type);
    EXPECT_STREQ("1!400001", req->tags[0].address);
    EXPECT_EQ(0, req->tags[0].attribute);

    EXPECT_STREQ("name2", req->tags[1].name);
    EXPECT_EQ(1, req->tags[1].type);
    EXPECT_STREQ("1!400002", req->tags[1].address);
    EXPECT_EQ(0, req->tags[1].attribute);

    neu_parse_decode_add_tags_free(req);
}

TEST(JsonDatatagTableTest, GetTagDecode)
{
    char *                    buf = (char *) "{\"node_id\": 123456}";
    neu_parse_get_tags_req_t *req = NULL;

    EXPECT_EQ(0, neu_parse_decode_get_tags(buf, &req));

    EXPECT_EQ(123456, req->node_id);

    neu_parse_decode_get_tags_free(req);
}

TEST(JsonDatatagTableTest, GetTagEncode)
{
    char *buf =
        (char
             *) "{\"tags\": [{\"name\": \"Tag0001\", \"type\": 0, \"address\": "
                "\"1!400001\", \"attribute\": 0, \"tag_id\": 0}, {\"name\": "
                "\"Tag0002\", \"type\": 1, \"address\": \"1!400002\", "
                "\"attribute\": 0, \"tag_id\": 0}]}";
    char *                   result = NULL;
    neu_parse_get_tags_res_t res    = { 0 };

    res.n_tag = 2;

    res.tags = (neu_parse_get_tags_res_tag_t *) calloc(
        2, sizeof(neu_parse_get_tags_res_tag_t));
    res.tags[0].name      = strdup((char *) "Tag0001");
    res.tags[0].type      = 0;
    res.tags[0].address   = strdup((char *) "1!400001");
    res.tags[0].attribute = 0;

    res.tags[1].name      = strdup((char *) "Tag0002");
    res.tags[1].type      = 1;
    res.tags[1].address   = strdup((char *) "1!400002");
    res.tags[1].attribute = 0;

    EXPECT_EQ(0,
              neu_json_encode_by_fn(&res, neu_parse_encode_get_tags, &result));
    EXPECT_STREQ(buf, result);

    free(res.tags[0].name);
    free(res.tags[1].name);
    free(res.tags[0].address);
    free(res.tags[1].address);
    free(res.tags);
    free(result);
}

TEST(JsonDatatagTableTest, DeleteTagDecode)
{
    char *buf =
        (char *) "{\"node_id\": 123456, \"group_config_name\": \"name\", "
                 "\"tag_ids\": [123, 456]}";
    neu_parse_del_tags_req_t *req = NULL;

    EXPECT_EQ(0, neu_parse_decode_del_tags(buf, &req));

    EXPECT_EQ(123456, req->node_id);
    EXPECT_STREQ("name", req->group_config_name);
    EXPECT_EQ(2, req->n_tag_id);
    EXPECT_EQ(123, req->tag_ids[0]);
    EXPECT_EQ(456, req->tag_ids[1]);

    neu_parse_decode_del_tags_free(req);
}

TEST(JsonDatatagTableTest, UpdateTaDecode)
{
    char *buf = (char *) "{\"node_id\":123456, "
                         "\"tag_id\":123, "
                         "\"tags\": "
                         "[{\"name\":\"name1\", \"type\":0,\"decimal\":1, "
                         "\"address\":\"1!400001\",\"flag\":0}, "
                         "{\"name\":\"name2\", \"type\":1,\"decimal\":1, "
                         "\"address\":\"1!400003\",\"flag\":0} ] }";
    neu_parse_update_tags_req_t *req = NULL;

    EXPECT_EQ(0, neu_parse_decode_update_tags(buf, &req));

    EXPECT_EQ(123456, req->node_id);

    EXPECT_STREQ("name1", req->tags[0].name);
    EXPECT_EQ(0, req->tags[0].type);
    EXPECT_STREQ("1!400001", req->tags[0].address);
    EXPECT_EQ(0, req->tags[0].attribute);

    EXPECT_STREQ("name2", req->tags[1].name);
    EXPECT_EQ(1, req->tags[1].type);
    EXPECT_STREQ("1!400003", req->tags[1].address);
    EXPECT_EQ(0, req->tags[1].attribute);

    neu_parse_decode_update_tags_free(req);
}
