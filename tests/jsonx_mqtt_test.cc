#include <stdlib.h>

#include <gtest/gtest.h>

#include "utils/neu_json.h"
#include "json/neu_json_fn.h"
#include "json/neu_json_mqtt.h"

TEST(JsonxMqttTest, mqttDecode)
{
    char *buf = (char *) "{\"function\": 1, "
                         "\"uuid\": \"123-456-abc\"}";
    neu_json_mqtt_param_t param = { 0 };
    int                   ret   = neu_json_decode_mqtt_param(buf, &param);

    EXPECT_EQ(ret, 0);

    neu_json_decode_mqtt_param_free(&param);
}

TEST(JsonxMqttTest, mqttEncode)
{
    char *buf    = (char *) "{\"function\": 1, \"uuid\": \"123-456-abc\"}";
    char *result = NULL;
    int   ret    = 0;
    neu_data_val_t *function = neu_dvalue_unit_new();
    neu_data_val_t *uuid     = neu_dvalue_unit_new();

    neu_dvalue_init_int16(function, 1);
    neu_dvalue_init_copy_cstr(uuid, (char *) "123-456-abc");
    neu_json_mqtt_param_t param = { .function = function, .uuid = uuid };

    ret = neu_json_encode_with_mqtt(NULL, NULL, &param,
                                    neu_json_encode_mqtt_param, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_STREQ(result, buf);

    free(result);
    neu_dvalue_free(function);
    neu_dvalue_free(uuid);
}
