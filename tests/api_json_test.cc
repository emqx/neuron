#include <stdlib.h>

#include <gtest/gtest.h>

#include "parser/neu_json_parser.h"
#include "parser/neu_json_read.h"
#include "parser/neu_json_write.h"

TEST(JsonAPITest, ReadReqDecode)
{
    char *buf = (char *) "{\"function\":50, "
                         "\"uuid\":\"554f5fd8-f437-11eb-975c-7704b9e17821\", "
                         "\"group\":\"Group0\", \"names\": [\"tag001\", "
                         "\"tag002\", \"tag003\"]}";
    void *                     result = NULL;
    struct neu_parse_read_req *req    = NULL;

    EXPECT_EQ(0, neu_parse_decode(buf, &result));

    req = (struct neu_parse_read_req *) result;
    EXPECT_EQ(NEU_PARSE_OP_READ, req->function);
    EXPECT_STREQ("554f5fd8-f437-11eb-975c-7704b9e17821", req->uuid);
    EXPECT_STREQ("Group0", req->group);
    EXPECT_EQ(3, req->n_name);
    EXPECT_STREQ("tag001", req->names[0].name);
    EXPECT_STREQ("tag002", req->names[1].name);
    EXPECT_STREQ("tag003", req->names[2].name);

    neu_parse_decode_free(result);
}

TEST(JsonAPITest, ReadResEncode)
{
    char *buf =
        (char
             *) "{\"function\": 50, \"uuid\": "
                "\"554f5fd8-f437-11eb-975c-7704b9e17821\", \"error\": 0, "
                "\"tags\": [{\"name\": \"tag001\", \"type\": 0, \"timestamp\": "
                "1122334455, \"value\": 123.0}, {\"name\": \"tag002\", "
                "\"type\": 1, \"timestamp\": 4445555, \"value\": 11233.0}]}";
    char *                    result = NULL;
    struct neu_parse_read_res res    = {
        .function = NEU_PARSE_OP_READ,
        .uuid     = (char *) "554f5fd8-f437-11eb-975c-7704b9e17821",
        .error    = 0,
        .n_tag    = 2,
    };

    res.tags = (struct neu_parse_read_res_tag *) calloc(
        2, sizeof(struct neu_parse_read_res_tag));
    res.tags[0].name      = strdup((char *) "tag001");
    res.tags[0].type      = 0;
    res.tags[0].timestamp = 1122334455;
    res.tags[0].value     = 123;

    res.tags[1].name      = strdup((char *) "tag002");
    res.tags[1].type      = 1;
    res.tags[1].timestamp = 4445555;
    res.tags[1].value     = 11233;

    EXPECT_EQ(0, neu_parse_encode(&res, &result));
    EXPECT_STREQ(buf, result);

    free(res.tags[0].name);
    free(res.tags[1].name);
    free(res.tags);
    free(result);
}

TEST(JsonAPITest, WriteResEncode)
{
    char *buf =
        (char *) "{\"function\": 51, \"uuid\": "
                 "\"554f5fd8-f437-11eb-975c-7704b9e17821\", \"error\": 0}";
    char *                     result = NULL;
    struct neu_parse_write_res res    = {
        .function = NEU_PARSE_OP_WRITE,
        .uuid     = (char *) "554f5fd8-f437-11eb-975c-7704b9e17821",
        .error    = 0,
    };

    EXPECT_EQ(0, neu_parse_encode(&res, &result));
    EXPECT_STREQ(buf, result);

    free(result);
}

TEST(JsonAPITest, WriteReqDecode)
{
    char *buf =
        (char *) "{\"function\":51, "
                 "\"uuid\":\"554f5fd8-f437-11eb-975c-7704b9e17821\", "
                 "\"group\":\"Group0\", \"tags\": "
                 "[{\"name\":\"name1\", \"value\":8877},{\"name\":\"name2\", "
                 "\"value\":11.22},{\"name\":\"name3\", \"value\": \"hello "
                 "world\"}]}";
    void *                      result = NULL;
    struct neu_parse_write_req *req    = NULL;

    EXPECT_EQ(0, neu_parse_decode(buf, &result));

    req = (struct neu_parse_write_req *) result;
    EXPECT_EQ(NEU_PARSE_OP_WRITE, req->function);
    EXPECT_STREQ("554f5fd8-f437-11eb-975c-7704b9e17821", req->uuid);
    EXPECT_STREQ("Group0", req->group);
    EXPECT_EQ(3, req->n_tag);

    EXPECT_STREQ("name1", req->tags[0].name);
    EXPECT_EQ(NEU_JSON_INT, req->tags[0].t);
    EXPECT_EQ(8877, req->tags[0].value.val_int);

    EXPECT_STREQ("name2", req->tags[1].name);
    EXPECT_EQ(NEU_JSON_DOUBLE, req->tags[1].t);
    EXPECT_EQ(11.22, req->tags[1].value.val_double);

    EXPECT_STREQ("name3", req->tags[2].name);
    EXPECT_EQ(NEU_JSON_STR, req->tags[2].t);
    EXPECT_STREQ("hello world", req->tags[2].value.val_str);

    neu_parse_decode_free(result);
}
