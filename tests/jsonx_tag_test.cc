#include <stdlib.h>

#include <gtest/gtest.h>

#include "utils/neu_json.h"
#include "json/neu_json_fn.h"
#include "json/neu_json_tag.h"

TEST(JsonxAddTagTest, AddTagDecode)
{
    char *buf =
        (char *) "{\"node_id\":1, \"group_config_name\":\"name1\", "
                 "\"tags\":[{\"name\":\"name1\", \"address\":\"address1\", "
                 "\"attribute\": 1, \"type\":1}, {\"name\": \"name2\", "
                 "\"address\": \"address2\", \"attribute\":2, \"type\":2}]}";
    neu_json_add_tags_req_t req = { 0 };
    int                     ret = neu_json_decode_add_tags(buf, &req);

    EXPECT_EQ(ret, 0);

    uint32_t node_id = 0;
    neu_dvalue_get_uint32(req.node_id, &node_id);
    EXPECT_EQ(node_id, 1);

    char *grp_name = NULL;
    neu_dvalue_get_cstr(req.group_config_name, &grp_name);
    EXPECT_STREQ(grp_name, "name1");
    free(grp_name);

    EXPECT_EQ(req.tags->length, 2);

    for (int i = 0; i < req.tags->length; i++) {
        neu_json_add_tags_req_tag_t *tag =
            (neu_json_add_tags_req_tag_t *) neu_fixed_array_get(req.tags, i);

        char *   tag_name  = NULL;
        char *   address   = NULL;
        uint16_t attribute = 0;
        uint16_t type      = 0;

        neu_dvalue_get_cstr(tag->name, &tag_name);
        neu_dvalue_get_cstr(tag->address, &address);
        neu_dvalue_get_uint16(tag->attribute, &attribute);
        neu_dvalue_get_uint16(tag->type, &type);

        if (i == 0) {
            EXPECT_STREQ(tag_name, "name1");
            EXPECT_STREQ(address, "address1");
            EXPECT_EQ(attribute, 1);
            EXPECT_EQ(type, 1);
        } else {
            EXPECT_STREQ(tag_name, "name2");
            EXPECT_STREQ(address, "address2");
            EXPECT_EQ(attribute, 2);
            EXPECT_EQ(type, 2);
        }

        free(tag_name);
        free(address);
    }

    neu_json_decode_add_tags_free(&req);
}
