#include <stdlib.h>

#include <gtest/gtest.h>

#include "json/neu_json_fn.h"
#include "json/neu_json_mqtt.h"
#include "json/neu_json_rw.h"

TEST(JsonAPITest, ReadReqDecode)
{
    char *buf = (char *) "{\"command\":\"\", "
                         "\"uuid\":\"554f5fd8-f437-11eb-975c-7704b9e17821\", "
                         "\"node_id\": 123 ,"
                         "\"group_config_name\": \"group1\"}";
    neu_json_read_req_t *req  = NULL;
    neu_json_mqtt_t *    mqtt = NULL;

    EXPECT_EQ(0, neu_json_decode_mqtt_req(buf, &mqtt));
    EXPECT_EQ(0, neu_json_decode_read_req(buf, &req));

    EXPECT_STREQ("554f5fd8-f437-11eb-975c-7704b9e17821", mqtt->uuid);
    EXPECT_EQ(123, req->node_id);
    EXPECT_STREQ("group1", req->group_config_name);

    neu_json_decode_mqtt_req_free(mqtt);
    neu_json_decode_read_req_free(req);
}

TEST(JsonAPITest, ReadResEncode)
{
    char *buf =
        (char *) "{\"tags\": [{\"value\": 123, \"id\": 1, \"error\": 0}, "
                 "{\"value\": 11.123456789, \"id\": 2, \"error\": 0}]}";
    char *               result = NULL;
    neu_json_read_resp_t res    = {
        .n_tag = 2,
    };

    res.tags = (neu_json_read_resp_tag_t *) calloc(
        2, sizeof(neu_json_read_resp_tag_t));
    res.tags[0].id            = 1;
    res.tags[0].t             = NEU_JSON_INT;
    res.tags[0].value.val_int = 123;

    res.tags[1].id               = 2;
    res.tags[1].t                = NEU_JSON_DOUBLE;
    res.tags[1].value.val_double = 11.123456789;

    EXPECT_EQ(0,
              neu_json_encode_by_fn(&res, neu_json_encode_read_resp, &result));
    EXPECT_STREQ(buf, result);

    free(res.tags);
    free(result);
}

TEST(JsonAPITest, WriteReqDecode)
{
    char *buf = (char *) "{\"node_id\": 123, \"group_config_name\": "
                         "\"config_opcua_sample\", \"tags\": "
                         "[{\"id\":1, \"value\":8877},{\"id\":2, "
                         "\"value\":11.22},{\"id\":3, \"value\": \"hello "
                         "world\"}]}";
    neu_json_write_req_t *req = NULL;

    EXPECT_EQ(0, neu_json_decode_write_req(buf, &req));

    EXPECT_EQ(3, req->n_tag);
    EXPECT_EQ(123, req->node_id);

    EXPECT_STREQ("config_opcua_sample", req->group_config_name);

    EXPECT_EQ(1, req->tags[0].id);
    EXPECT_EQ(NEU_JSON_INT, req->tags[0].t);
    EXPECT_EQ(8877, req->tags[0].value.val_int);

    EXPECT_EQ(2, req->tags[1].id);
    EXPECT_EQ(NEU_JSON_DOUBLE, req->tags[1].t);
    EXPECT_EQ(11.22, req->tags[1].value.val_double);

    EXPECT_EQ(3, req->tags[2].id);
    EXPECT_EQ(NEU_JSON_STR, req->tags[2].t);
    EXPECT_STREQ("hello world", req->tags[2].value.val_str);

    neu_json_decode_write_req_free(req);
}
