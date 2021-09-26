#include <stdlib.h>

#include <gtest/gtest.h>

#include "parser/neu_json_fn.h"
#include "parser/neu_json_plugin.h"

TEST(JsonPluginTest, AddPluginDecode)
{
    char *buf = (char *) "{\"kind\": 1, "
                         "\"node_type\": 1, "
                         "\"name\": \"plugin1\", "
                         "\"lib_name\": \"lib1\" }";
    neu_parse_add_plugin_req_t *req = NULL;

    EXPECT_EQ(0, neu_parse_decode_add_plugin(buf, &req));

    EXPECT_EQ(1, req->kind);
    EXPECT_EQ(1, req->node_type);
    EXPECT_STREQ("plugin1", req->plugin_name);
    EXPECT_STREQ("lib1", req->plugin_lib_name);

    neu_parse_decode_add_plugin_free(req);
}

TEST(JsonPluginTest, DeletePluginDecode)
{
    char *                      buf = (char *) "{\"id\": 1 }";
    neu_parse_del_plugin_req_t *req = NULL;

    EXPECT_EQ(0, neu_parse_decode_del_plugin(buf, &req));

    EXPECT_EQ(1, req->plugin_id);

    neu_parse_decode_del_plugin_free(req);
}

TEST(JsonPluginTest, UpdatePluginDecode)
{
    char *buf = (char *) "{\"kind\": 2, "
                         "\"node_type\": 2, "
                         "\"name\": \"plugin1\", "
                         "\"lib_name\": \"lib1\" }";
    neu_parse_update_plugin_req_t *req = NULL;

    EXPECT_EQ(0, neu_parse_decode_update_plugin(buf, &req));

    EXPECT_EQ(2, req->kind);
    EXPECT_EQ(2, req->node_type);
    EXPECT_STREQ("plugin1", req->plugin_name);
    EXPECT_STREQ("lib1", req->plugin_lib_name);

    neu_parse_decode_update_plugin_free(req);
}

TEST(JsonPluginTest, GetPluginEncode)
{
    char *buf =
        (char *) "{\"plugin_libs\": [{\"id\": 123, \"kind\": 1, \"node_type\": "
                 "1, \"name\": \"plugin1\", \"lib_name\": \"lib1\"}, {\"id\": "
                 "456, \"kind\": 1, \"node_type\": 1, \"name\": \"plugin2\", "
                 "\"lib_name\": \"lib2\"}, {\"id\": 789, \"kind\": 1, "
                 "\"node_type\": 1, \"name\": \"plugin3\", \"lib_name\": "
                 "\"lib3\"}]}";
    char *result = NULL;

    neu_parse_get_plugins_res_t res = {
        .n_plugin = 3,
    };

    res.plugin_libs = (neu_parse_get_plugins_res_lib_t *) calloc(
        3, sizeof(neu_parse_get_plugins_res_lib_t));

    res.plugin_libs[0].plugin_id       = 123;
    res.plugin_libs[0].kind            = PLUGIN_KIND_SYSTEM;
    res.plugin_libs[0].node_type       = NEU_NODE_TYPE_DRIVER;
    res.plugin_libs[0].plugin_name     = strdup((char *) "plugin1");
    res.plugin_libs[0].plugin_lib_name = strdup((char *) "lib1");

    res.plugin_libs[1].plugin_id       = 456;
    res.plugin_libs[1].kind            = PLUGIN_KIND_SYSTEM;
    res.plugin_libs[1].node_type       = NEU_NODE_TYPE_DRIVER;
    res.plugin_libs[1].plugin_name     = strdup((char *) "plugin2");
    res.plugin_libs[1].plugin_lib_name = strdup((char *) "lib2");

    res.plugin_libs[2].plugin_id       = 789;
    res.plugin_libs[2].kind            = PLUGIN_KIND_SYSTEM;
    res.plugin_libs[2].node_type       = NEU_NODE_TYPE_DRIVER;
    res.plugin_libs[2].plugin_name     = strdup((char *) "plugin3");
    res.plugin_libs[2].plugin_lib_name = strdup((char *) "lib3");

    EXPECT_EQ(
        0, neu_json_encode_by_fn(&res, neu_parse_encode_get_plugins, &result));
    EXPECT_STREQ(buf, result);

    free(res.plugin_libs[0].plugin_name);
    free(res.plugin_libs[0].plugin_lib_name);
    free(res.plugin_libs[1].plugin_name);
    free(res.plugin_libs[1].plugin_lib_name);
    free(res.plugin_libs[2].plugin_name);
    free(res.plugin_libs[2].plugin_lib_name);
    free(res.plugin_libs);

    free(result);
}