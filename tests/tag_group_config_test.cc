#include <cstdlib>
#include <gtest/gtest.h>

#include "parser/neu_json_parser.h"
#include "parser/neu_json_tag_group.h"

TEST(TagGroupConfigTest, neu_paser_decode_read_tag_group_list_req)
{
    char *json = (char *) "{ \"function\": 29, \"uuid\": "
                          "\"1d892fe8-f37e-11eb-9a34-932aa06a28f3\", "
                          "\"config\": \"Config0\" }";

    void *                                    result = NULL;
    struct neu_paser_read_tag_group_list_req *req    = NULL;

    EXPECT_EQ(0, neu_parse_decode(json, &result));

    req = (struct neu_paser_read_tag_group_list_req *) result;
    EXPECT_EQ(NEU_PARSE_OP_READ_TAG_GROUP_LIST, req->function);
    EXPECT_STREQ("1d892fe8-f37e-11eb-9a34-932aa06a28f3", req->uuid);
    EXPECT_STREQ("Config0", req->config);
    neu_parse_decode_free(result);
}

TEST(TagGroupConfigTest, neu_paser_decode_read_tag_group_list_res)
{
    char *buf = (char *) "{\"function\": 29, \"uuid\": "
                         "\"1d892fe8-f37e-11eb-9a34-932aa06a28f3\", \"error\": "
                         "0, \"groups\": [{\"name\": \"Group0\"}, {\"name\": "
                         "\"Group1\"}, {\"name\": \"Group2\"}]}";
    char *                                   result = NULL;
    struct neu_paser_read_tag_group_list_res res    = {
        .function = NEU_PARSE_OP_READ_TAG_GROUP_LIST,
        .uuid     = (char *) "1d892fe8-f37e-11eb-9a34-932aa06a28f3",
        .error    = 0,
        .n_group  = 3,
    };

    res.names = (struct neu_paser_read_tag_group_list_name *) calloc(
        3, sizeof(struct neu_paser_read_tag_group_list_name));

    res.names[0].name = strdup((char *) "Group0");
    res.names[1].name = strdup((char *) "Group1");
    res.names[2].name = strdup((char *) "Group2");

    EXPECT_EQ(0, neu_parse_encode(&res, &result));
    EXPECT_STREQ(buf, result);

    free(res.names[0].name);
    free(res.names[1].name);
    free(res.names[2].name);
    free(res.names);
    free(result);
}