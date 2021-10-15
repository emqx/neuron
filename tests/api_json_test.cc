#include <stdlib.h>

#include <gtest/gtest.h>

#include "parser/neu_json_fn.h"
#include "parser/neu_json_mqtt.h"
#include "parser/neu_json_rw.h"

TEST(JsonAPITest, ReadReqDecode)
{
    char *buf = (char *) "{\"function\":3, "
                         "\"uuid\":\"554f5fd8-f437-11eb-975c-7704b9e17821\", "
                         "\"node_id\": 123 ,"
                         "\"group_config_name\": \"group1\"}";
    neu_parse_read_req_t *req  = NULL;
    neu_parse_mqtt_t *    mqtt = NULL;

    EXPECT_EQ(0, neu_parse_decode_mqtt_param(buf, &mqtt));
    EXPECT_EQ(0, neu_parse_decode_read(buf, &req));

    EXPECT_EQ(NEU_MQTT_OP_READ, mqtt->function);
    EXPECT_STREQ("554f5fd8-f437-11eb-975c-7704b9e17821", mqtt->uuid);
    EXPECT_EQ(123, req->node_id);
    EXPECT_STREQ("group1", req->group_config_name);

    neu_parse_decode_mqtt_param_free(mqtt);
    neu_parse_decode_read_free(req);
}

TEST(JsonAPITest, ReadResEncode)
{
    char *buf = (char *) "{\"tags\": [{\"id\": 1, "
                         "\"value\": 123}, {\"id\": "
                         "2, \"value\": 11.123456789}]}";
    char *               result = NULL;
    neu_parse_read_res_t res    = {
        .n_tag = 2,
    };

    res.tags = (neu_parse_read_res_tag_t *) calloc(
        2, sizeof(neu_parse_read_res_tag_t));
    res.tags[0].tag_id        = 1;
    res.tags[0].t             = NEU_JSON_INT;
    res.tags[0].value.val_int = 123;

    res.tags[1].tag_id           = 2;
    res.tags[1].t                = NEU_JSON_DOUBLE;
    res.tags[1].value.val_double = 11.123456789;

    EXPECT_EQ(0, neu_json_encode_by_fn(&res, neu_parse_encode_read, &result));
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
    neu_parse_write_req_t *req = NULL;

    EXPECT_EQ(0, neu_parse_decode_write(buf, &req));

    EXPECT_EQ(3, req->n_tag);
    EXPECT_EQ(123, req->node_id);

    EXPECT_STREQ("config_opcua_sample", req->group_config_name);

    EXPECT_EQ(1, req->tags[0].tag_id);
    EXPECT_EQ(NEU_JSON_INT, req->tags[0].t);
    EXPECT_EQ(8877, req->tags[0].value.val_int);

    EXPECT_EQ(2, req->tags[1].tag_id);
    EXPECT_EQ(NEU_JSON_DOUBLE, req->tags[1].t);
    EXPECT_EQ(11.22, req->tags[1].value.val_double);

    EXPECT_EQ(3, req->tags[2].tag_id);
    EXPECT_EQ(NEU_JSON_STR, req->tags[2].t);
    EXPECT_STREQ("hello world", req->tags[2].value.val_str);

    neu_parse_decode_write_free(req);
}
