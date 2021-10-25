#include <stdlib.h>

#include <gtest/gtest.h>

#include "schema/schema.h"
#include "utils/json.h"

TEST(JsonSchema, SchemaValidTagType)
{
    char  buf[40960] = { 0 };
    FILE *fp         = fopen("./plugin_param_schema.json", "r");
    fread(buf, 1, sizeof(buf), fp);
    fclose(fp);

    neu_schema_valid_t *sv = neu_schema_load(buf, (char *) "test-plugin");

    EXPECT_EQ(-1, neu_schema_valid_tag_type(sv, 1));
    EXPECT_EQ(0, neu_schema_valid_tag_type(sv, 4));

    neu_schema_free(sv);
}

TEST(JsonSchema, SchemaValidInt)
{
    char  buf[40960] = { 0 };
    FILE *fp         = fopen("./plugin_param_schema.json", "r");
    fread(buf, 1, sizeof(buf), fp);
    fclose(fp);

    neu_schema_valid_t *sv = neu_schema_load(buf, (char *) "test-plugin");

    EXPECT_EQ(0, neu_schema_valid_param_int(sv, 1234, (char *) "port"));

    EXPECT_EQ(-1, neu_schema_valid_param_int(sv, 1, (char *) "port"));

    neu_schema_free(sv);
}

TEST(JsonSchema, SchemaValidString)
{
    char  buf[40960] = { 0 };
    FILE *fp         = fopen("./plugin_param_schema.json", "r");
    fread(buf, 1, sizeof(buf), fp);
    fclose(fp);

    neu_schema_valid_t *sv = neu_schema_load(buf, (char *) "test-plugin");

    char *str1 = (char *) "1234";
    char *str2 = (char *) "1234567890123456789012345678901";

    EXPECT_EQ(0, neu_schema_valid_param_string(sv, str1, (char *) "host"));

    EXPECT_EQ(-1, neu_schema_valid_param_string(sv, str2, (char *) "host"));

    neu_schema_free(sv);
}

TEST(JsonSchema, SchemaValidReal)
{
    char  buf[40960] = { 0 };
    FILE *fp         = fopen("./plugin_param_schema.json", "r");
    fread(buf, 1, sizeof(buf), fp);
    fclose(fp);

    neu_schema_valid_t *sv = neu_schema_load(buf, (char *) "test-plugin");

    EXPECT_EQ(0, neu_schema_valid_param_real(sv, 10.1, (char *) "real_param"));

    EXPECT_EQ(-1, neu_schema_valid_param_real(sv, 30.5, (char *) "real_param"));

    neu_schema_free(sv);
}

TEST(JsonSchema, SchemaValidEnum)
{
    char  buf[40960] = { 0 };
    FILE *fp         = fopen("./plugin_param_schema.json", "r");
    fread(buf, 1, sizeof(buf), fp);
    fclose(fp);

    neu_schema_valid_t *sv = neu_schema_load(buf, (char *) "test-plugin");

    EXPECT_EQ(0, neu_schema_valid_param_enum(sv, 9600, (char *) "baud_rate"));

    EXPECT_EQ(-1, neu_schema_valid_param_enum(sv, 1234, (char *) "baud_rate"));

    neu_schema_free(sv);
}