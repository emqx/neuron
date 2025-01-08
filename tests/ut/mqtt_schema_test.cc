#include <gtest/gtest.h>

#include "neuron.h"

#include "mqtt/schema.h"

int64_t          global_timestamp = 0;
zlog_category_t *neuron           = NULL;

TEST(validate_success, schema)
{
    const char *schema =
        "{\"timestamp\":\"${timestamp}\",\"node\":\"${node}\",\"group\":\"${"
        "group}\",\"tags\":\"${tag_values}\",\"static\":\"${static_"
        "tags}\",\"errors\":\"${tag_errors}\"}";
    mqtt_schema_vt_t *vts;
    size_t            n_vts;
    int               ret = mqtt_schema_validate(schema, &vts, &n_vts);
    EXPECT_EQ(ret, 0);

    EXPECT_EQ(n_vts, 6);
    free(vts);
}

int main(int argc, char **argv)
{
    zlog_init("./config/dev.conf");
    neuron = zlog_get_category("neuron");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
