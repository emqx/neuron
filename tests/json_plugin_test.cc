#include <stdlib.h>

#include <gtest/gtest.h>

#include "json/neu_json_fn.h"
#include "json/neu_json_plugin.h"

#include "adapter.h"

TEST(JsonPluginTest, AddPluginDecode)
{
    char *                     buf = (char *) "{\"lib_name\": \"lib1\" }";
    neu_json_add_plugin_req_t *req = NULL;

    EXPECT_EQ(0, neu_json_decode_add_plugin_req(buf, &req));

    EXPECT_STREQ("lib1", req->lib_name);

    neu_json_decode_add_plugin_req_free(req);
}

TEST(JsonPluginTest, DeletePluginDecode)
{
    char *                     buf = (char *) "{\"id\": 1 }";
    neu_json_del_plugin_req_t *req = NULL;

    EXPECT_EQ(0, neu_json_decode_del_plugin_req(buf, &req));

    EXPECT_EQ(1, req->id);

    neu_json_decode_del_plugin_req_free(req);
}

TEST(JsonPluginTest, GetPluginEncode)
{
    char *buf =
        (char *) "{\"plugin_libs\": [{\"node_type\": 1, \"name\": \"plugin1\", "
                 "\"lib_name\": \"lib1\", \"kind\": 1, \"id\": 123}, "
                 "{\"node_type\": 1, \"name\": \"plugin2\", \"lib_name\": "
                 "\"lib2\", \"kind\": 1, \"id\": 456}, {\"node_type\": 1, "
                 "\"name\": \"plugin3\", \"lib_name\": \"lib3\", \"kind\": 1, "
                 "\"id\": 789}]}";
    char *result = NULL;

    neu_json_get_plugin_resp_t res = {
        .n_plugin_lib = 3,
    };

    res.plugin_libs = (neu_json_get_plugin_resp_plugin_lib_t *) calloc(
        3, sizeof(neu_json_get_plugin_resp_plugin_lib_t));

    res.plugin_libs[0].id        = 123;
    res.plugin_libs[0].kind      = PLUGIN_KIND_SYSTEM;
    res.plugin_libs[0].node_type = NEU_NODE_TYPE_DRIVER;
    res.plugin_libs[0].name      = strdup((char *) "plugin1");
    res.plugin_libs[0].lib_name  = strdup((char *) "lib1");

    res.plugin_libs[1].id        = 456;
    res.plugin_libs[1].kind      = PLUGIN_KIND_SYSTEM;
    res.plugin_libs[1].node_type = NEU_NODE_TYPE_DRIVER;
    res.plugin_libs[1].name      = strdup((char *) "plugin2");
    res.plugin_libs[1].lib_name  = strdup((char *) "lib2");

    res.plugin_libs[2].id        = 789;
    res.plugin_libs[2].kind      = PLUGIN_KIND_SYSTEM;
    res.plugin_libs[2].node_type = NEU_NODE_TYPE_DRIVER;
    res.plugin_libs[2].name      = strdup((char *) "plugin3");
    res.plugin_libs[2].lib_name  = strdup((char *) "lib3");

    EXPECT_EQ(
        0,
        neu_json_encode_by_fn(&res, neu_json_encode_get_plugin_resp, &result));
    EXPECT_STREQ(buf, result);

    free(res.plugin_libs[0].name);
    free(res.plugin_libs[0].lib_name);
    free(res.plugin_libs[1].name);
    free(res.plugin_libs[1].lib_name);
    free(res.plugin_libs[2].name);
    free(res.plugin_libs[2].lib_name);
    free(res.plugin_libs);

    free(result);
}