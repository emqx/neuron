#include <stdlib.h>

#include <gtest/gtest.h>

#include "parser/neu_json_parser.h"
#include "parser/neu_json_plugin.h"

TEST(JsonPluginTest, AddPluginDecode)
{
    char *buf = (char *) "{\"function\": 39, "
                         "\"uuid\": \"554f5fd8-f437-11eb-975c-7704b9e17821\", "
                         "\"kind\": 1, "
                         "\"node_type\": 1, "
                         "\"plugin_name\": \"plugin1\", "
                         "\"plugin_lib_name\": \"lib1\" }";
    void *                           result = NULL;
    struct neu_parse_add_plugin_req *req    = NULL;

    EXPECT_EQ(0, neu_parse_decode(buf, &result));

    req = (struct neu_parse_add_plugin_req *) result;

    EXPECT_EQ(NEU_PARSE_OP_ADD_PLUGIN, req->function);
    EXPECT_STREQ("554f5fd8-f437-11eb-975c-7704b9e17821", req->uuid);
    EXPECT_EQ(1, req->kind);
    EXPECT_EQ(1, req->node_type);
    EXPECT_STREQ("plugin1", req->plugin_name);
    EXPECT_STREQ("lib1", req->plugin_lib_name);

    neu_parse_decode_free(result);
}

TEST(JsonPluginTest, AddPluginEncode)
{
    char *buf = (char *) "{\"function\": 39, "
                         "\"uuid\": \"554f5fd8-f437-11eb-975c-7704b9e17821\", "
                         "\"error\": 0}";
    char *                          result = NULL;
    struct neu_parse_add_plugin_res res    = {
        .function = NEU_PARSE_OP_ADD_PLUGIN,
        .uuid     = (char *) "554f5fd8-f437-11eb-975c-7704b9e17821",
        .error    = 0,
    };

    EXPECT_EQ(0, neu_parse_encode(&res, &result));
    EXPECT_STREQ(buf, result);

    free(result);
}

TEST(JsonPluginTest, DeletePluginDecode)
{
    char *buf = (char *) "{\"function\": 41, "
                         "\"uuid\": \"554f5fd8-f437-11eb-975c-7704b9e17821\", "
                         "\"plugin_id\": 1 }";
    void *                              result = NULL;
    struct neu_parse_delete_plugin_req *req    = NULL;

    EXPECT_EQ(0, neu_parse_decode(buf, &result));

    req = (struct neu_parse_delete_plugin_req *) result;

    EXPECT_EQ(NEU_PARSE_OP_DELETE_PLUGIN, req->function);
    EXPECT_STREQ("554f5fd8-f437-11eb-975c-7704b9e17821", req->uuid);
    EXPECT_EQ(1, req->plugin_id);

    neu_parse_decode_free(result);
}

TEST(JsonPluginTest, DeletePluginEncode)
{
    char *buf = (char *) "{\"function\": 41, "
                         "\"uuid\": \"554f5fd8-f437-11eb-975c-7704b9e17821\", "
                         "\"error\": 0}";
    char *                             result = NULL;
    struct neu_parse_delete_plugin_res res    = {
        .function = NEU_PARSE_OP_DELETE_PLUGIN,
        .uuid     = (char *) "554f5fd8-f437-11eb-975c-7704b9e17821",
        .error    = 0,
    };

    EXPECT_EQ(0, neu_parse_encode(&res, &result));
    EXPECT_STREQ(buf, result);

    free(result);
}

TEST(JsonPluginTest, UpdatePluginDecode)
{
    char *buf = (char *) "{\"function\": 42, "
                         "\"uuid\": \"554f5fd8-f437-11eb-975c-7704b9e17821\", "
                         "\"kind\": 2, "
                         "\"node_type\": 2, "
                         "\"plugin_name\": \"plugin1\", "
                         "\"plugin_lib_name\": \"lib1\" }";
    void *                              result = NULL;
    struct neu_parse_update_plugin_req *req    = NULL;

    EXPECT_EQ(0, neu_parse_decode(buf, &result));

    req = (struct neu_parse_update_plugin_req *) result;

    EXPECT_EQ(NEU_PARSE_OP_UPDATE_PLUGIN, req->function);
    EXPECT_STREQ("554f5fd8-f437-11eb-975c-7704b9e17821", req->uuid);
    EXPECT_EQ(2, req->kind);
    EXPECT_EQ(2, req->node_type);
    EXPECT_STREQ("plugin1", req->plugin_name);
    EXPECT_STREQ("lib1", req->plugin_lib_name);

    neu_parse_decode_free(result);
}

TEST(JsonPluginTest, UpdatePluginEncode)
{
    char *buf = (char *) "{\"function\": 42, "
                         "\"uuid\": \"554f5fd8-f437-11eb-975c-7704b9e17821\", "
                         "\"error\": 0}";
    char *                          result = NULL;
    struct neu_parse_add_plugin_res res    = {
        .function = NEU_PARSE_OP_UPDATE_PLUGIN,
        .uuid     = (char *) "554f5fd8-f437-11eb-975c-7704b9e17821",
        .error    = 0,
    };

    EXPECT_EQ(0, neu_parse_encode(&res, &result));
    EXPECT_STREQ(buf, result);

    free(result);
}

TEST(JsonPluginTest, GetPluginDecode)
{
    char *buf = (char *) "{\"function\": 40, "
                         "\"uuid\": \"554f5fd8-f437-11eb-975c-7704b9e17821\" }";
    void *                           result = NULL;
    struct neu_parse_get_plugin_req *req    = NULL;

    EXPECT_EQ(0, neu_parse_decode(buf, &result));

    req = (struct neu_parse_get_plugin_req *) result;

    EXPECT_EQ(NEU_PARSE_OP_GET_PLUGIN, req->function);
    EXPECT_STREQ("554f5fd8-f437-11eb-975c-7704b9e17821", req->uuid);

    neu_parse_decode_free(result);
}

TEST(JsonPluginTest, GetPluginEncode)
{
    char *buf = (char *) "{\"function\": 40, "
                         "\"uuid\": \"554f5fd8-f437-11eb-975c-7704b9e17821\", "
                         "\"plugin_id\": 123, "
                         "\"kind\": 1, "
                         "\"node_type\": 1, "
                         "\"plugin_name\": \"plugin1\", "
                         "\"plugin_lib_name\": \"lib1\"}";
    char *                          result = NULL;
    struct neu_parse_get_plugin_res res;
    res.function        = NEU_PARSE_OP_GET_PLUGIN;
    res.uuid            = (char *) "554f5fd8-f437-11eb-975c-7704b9e17821";
    res.plugin_id       = 123;
    res.kind            = PLUGIN_KIND_SYSTEM;
    res.node_type       = NEU_NODE_TYPE_DRIVER;
    res.plugin_name     = (char *) "plugin1";
    res.plugin_lib_name = (char *) "lib1";

    EXPECT_EQ(0, neu_parse_encode(&res, &result));
    EXPECT_STREQ(buf, result);

    free(result);
}