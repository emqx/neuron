#include <stdlib.h>

#include <gtest/gtest.h>

#include "json/neu_json_fn.h"
#include "json/neu_json_mqtt.h"
#include "json/neu_json_rw.h"

TEST(JsonAPITest, ReadReqDecode)
{
    char *buf = (char *) "{\"command\":\"\", "
                         "\"uuid\":\"554f5fd8-f437-11eb-975c-7704b9e17821\", "
                         "\"node_name\": \"name1\" ,"
                         "\"group_name\": \"group1\"}";
    neu_json_read_req_t *req  = NULL;
    neu_json_mqtt_t *    mqtt = NULL;

    EXPECT_EQ(0, neu_json_decode_mqtt_req(buf, &mqtt));
    EXPECT_EQ(0, neu_json_decode_read_req(buf, &req));

    EXPECT_STREQ("554f5fd8-f437-11eb-975c-7704b9e17821", mqtt->uuid);
    EXPECT_STREQ("name1", req->node_name);
    EXPECT_STREQ("group1", req->group_name);

    neu_json_decode_mqtt_req_free(mqtt);
    neu_json_decode_read_req_free(req);
}

TEST(JsonAPITest, ReadResEncode)
{
    char *buf = (char *) "{\"tags\": [{\"name\": \"data1\", \"value\": 123}, "
                         "{\"name\": \"data2\", \"value\": 11.123456789}]}";
    char *               result = NULL;
    neu_json_read_resp_t res    = {
        .n_tag = 2,
    };

    res.tags = (neu_json_read_resp_tag_t *) calloc(
        2, sizeof(neu_json_read_resp_tag_t));
    res.tags[0].name          = strdup((char *) "data1");
    res.tags[0].t             = NEU_JSON_INT;
    res.tags[0].value.val_int = 123;

    res.tags[1].name             = strdup((char *) "data2");
    res.tags[1].t                = NEU_JSON_DOUBLE;
    res.tags[1].value.val_double = 11.123456789;

    EXPECT_EQ(0,
              neu_json_encode_by_fn(&res, neu_json_encode_read_resp, &result));
    EXPECT_STREQ(buf, result);

    free(res.tags[0].name);
    free(res.tags[1].name);
    free(res.tags);
    free(result);
}

TEST(JsonAPITest, WriteReqDecode)
{
    char *buf = (char *) "{\"node_name\": \"name1\", \"group_name\": "
                         "\"config_opcua_sample\", \"tag_name\": \"name1\", "
                         "\"value\": \"hello world\"}";
    neu_json_write_req_t *req = NULL;

    EXPECT_EQ(0, neu_json_decode_write_req(buf, &req));

    EXPECT_STREQ("name1", req->node_name);

    EXPECT_STREQ("config_opcua_sample", req->group_name);

    EXPECT_STREQ("name1", req->tag_name);
    EXPECT_STREQ("hello world", req->value.val_str);

    neu_json_decode_write_req_free(req);
}
