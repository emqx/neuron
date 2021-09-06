#include <stdlib.h>

#include <gtest/gtest.h>

#include "parser/neu_json_parser.h"
#include "parser/neu_json_tag.h"

TEST(JsonDatatagTableTest, AddTagDecode)
{
    char *buf = (char *) "{\"function\":31, "
                         "\"uuid\":\"554f5fd8-f437-11eb-975c-7704b9e17821\", "
                         "\"node_id\":123456, \"group_config_name\": \"name\","
                         "\"tags\": "
                         "[{\"name\":\"name1\", \"type\":0,\"decimal\":1, "
                         "\"address\":\"1!400001\",\"flag\":0}, "
                         "{\"name\":\"name2\", \"type\":1,\"decimal\":1, "
                         "\"address\":\"1!400002\",\"flag\":0} ] }";
    void *                         result = NULL;
    struct neu_parse_add_tags_req *req    = NULL;

    EXPECT_EQ(0, neu_parse_decode(buf, &result));

    req = (struct neu_parse_add_tags_req *) result;

    EXPECT_EQ(NEU_PARSE_OP_ADD_TAGS, req->function);
    EXPECT_STREQ("554f5fd8-f437-11eb-975c-7704b9e17821", req->uuid);
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

    neu_parse_decode_free(result);
}

TEST(JsonDatatagTableTest, AddTagEncode)
{
    char *buf = (char *) "{\"function\": 31, "
                         "\"uuid\": \"554f5fd8-f437-11eb-975c-7704b9e17821\", "
                         "\"error\": 0}";
    char *                        result = NULL;
    struct neu_parse_add_tags_res res    = {
        .function = NEU_PARSE_OP_ADD_TAGS,
        .uuid     = (char *) "554f5fd8-f437-11eb-975c-7704b9e17821",
        .error    = 0,
    };

    EXPECT_EQ(0, neu_parse_encode(&res, &result));
    EXPECT_STREQ(buf, result);

    free(result);
}

TEST(JsonDatatagTableTest, GetTagDecode)
{
    char *buf = (char *) "{\"function\": 32, "
                         "\"uuid\": \"554f5fd8-f437-11eb-975c-7704b9e17821\", "
                         "\"node_id\": 123456}";
    void *                         result = NULL;
    struct neu_parse_get_tags_req *req    = NULL;

    EXPECT_EQ(0, neu_parse_decode(buf, &result));

    req = (struct neu_parse_get_tags_req *) result;

    EXPECT_EQ(NEU_PARSE_OP_GET_TAGS, req->function);
    EXPECT_STREQ("554f5fd8-f437-11eb-975c-7704b9e17821", req->uuid);
    EXPECT_EQ(123456, req->node_id);

    neu_parse_decode_free(result);
}

TEST(JsonDatatagTableTest, GetTagEncode)
{
    char *buf =
        (char *) "{\"function\": 32, \"uuid\": "
                 "\"554f5fd8-f437-11eb-975c-7704b9e17821\", \"error\": 0, "
                 "\"tags\": [{\"name\": \"Tag0001\", \"type\": 0, \"address\": "
                 "\"1!400001\", \"attribute\": 0}, {\"name\": \"Tag0002\", "
                 "\"type\": 1, \"address\": \"1!400002\", \"attribute\": 0}]}";
    char *                        result = NULL;
    struct neu_parse_get_tags_res res    = {
        .function = NEU_PARSE_OP_GET_TAGS,
        .uuid     = (char *) "554f5fd8-f437-11eb-975c-7704b9e17821",
        .error    = 0,
        .n_tag    = 2,
    };

    res.tags = (struct neu_parse_get_tags_res_tag *) calloc(
        2, sizeof(struct neu_parse_get_tags_res_tag));
    res.tags[0].name      = strdup((char *) "Tag0001");
    res.tags[0].type      = 0;
    res.tags[0].address   = strdup((char *) "1!400001");
    res.tags[0].attribute = 0;

    res.tags[1].name      = strdup((char *) "Tag0002");
    res.tags[1].type      = 1;
    res.tags[1].address   = strdup((char *) "1!400002");
    res.tags[1].attribute = 0;

    EXPECT_EQ(0, neu_parse_encode(&res, &result));
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
        (char *) "{\"function\": 33, "
                 "\"uuid\": \"554f5fd8-f437-11eb-975c-7704b9e17821\", "
                 "\"node_id\": 123456, \"group_config_name\": \"name\", "
                 "\"tag_ids\": [123, 456]}";
    void *                            result = NULL;
    struct neu_parse_delete_tags_req *req    = NULL;

    EXPECT_EQ(0, neu_parse_decode(buf, &result));

    req = (struct neu_parse_delete_tags_req *) result;

    EXPECT_EQ(NEU_PARSE_OP_DELETE_TAGS, req->function);
    EXPECT_STREQ("554f5fd8-f437-11eb-975c-7704b9e17821", req->uuid);
    EXPECT_EQ(123456, req->node_id);
    EXPECT_STREQ("name", req->group_config_name);
    EXPECT_EQ(2, req->n_tag_id);
    EXPECT_EQ(123, req->tag_ids[0]);
    EXPECT_EQ(456, req->tag_ids[1]);

    neu_parse_decode_free(result);
}

TEST(JsonDatatagTableTest, DeleteTagEncode)
{
    char *buf = (char *) "{\"function\": 33, "
                         "\"uuid\": \"554f5fd8-f437-11eb-975c-7704b9e17821\", "
                         "\"error\": 0}";
    char *                           result = NULL;
    struct neu_parse_delete_tags_res res    = {
        .function = NEU_PARSE_OP_DELETE_TAGS,
        .uuid     = (char *) "554f5fd8-f437-11eb-975c-7704b9e17821",
        .error    = 0,
    };

    EXPECT_EQ(0, neu_parse_encode(&res, &result));
    EXPECT_STREQ(buf, result);

    free(result);
}

TEST(JsonDatatagTableTest, UpdateTaDecode)
{
    char *buf = (char *) "{\"function\":34, "
                         "\"uuid\":\"554f5fd8-f437-11eb-975c-7704b9e17821\", "
                         "\"node_id\":123456, "
                         "\"tag_id\":123, "
                         "\"tags\": "
                         "[{\"name\":\"name1\", \"type\":0,\"decimal\":1, "
                         "\"address\":\"1!400001\",\"flag\":0}, "
                         "{\"name\":\"name2\", \"type\":1,\"decimal\":1, "
                         "\"address\":\"1!400003\",\"flag\":0} ] }";
    void *                            result = NULL;
    struct neu_parse_update_tags_req *req    = NULL;

    EXPECT_EQ(0, neu_parse_decode(buf, &result));

    req = (struct neu_parse_update_tags_req *) result;

    EXPECT_EQ(NEU_PARSE_OP_UPDATE_TAGS, req->function);
    EXPECT_STREQ("554f5fd8-f437-11eb-975c-7704b9e17821", req->uuid);
    EXPECT_EQ(123456, req->node_id);

    EXPECT_STREQ("name1", req->tags[0].name);
    EXPECT_EQ(0, req->tags[0].type);
    EXPECT_STREQ("1!400001", req->tags[0].address);
    EXPECT_EQ(0, req->tags[0].attribute);

    EXPECT_STREQ("name2", req->tags[1].name);
    EXPECT_EQ(1, req->tags[1].type);
    EXPECT_STREQ("1!400003", req->tags[1].address);
    EXPECT_EQ(0, req->tags[1].attribute);

    neu_parse_decode_free(result);
}

TEST(JsonDatatagTableTest, UpdateTagEncode)
{
    char *buf = (char *) "{\"function\": 34, "
                         "\"uuid\": \"554f5fd8-f437-11eb-975c-7704b9e17821\", "
                         "\"error\": 0}";
    char *                           result = NULL;
    struct neu_parse_update_tags_res res    = {
        .function = NEU_PARSE_OP_UPDATE_TAGS,
        .uuid     = (char *) "554f5fd8-f437-11eb-975c-7704b9e17821",
        .error    = 0,
    };

    EXPECT_EQ(0, neu_parse_encode(&res, &result));
    EXPECT_STREQ(buf, result);

    free(result);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}