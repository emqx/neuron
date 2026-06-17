#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "utils/ede.h"
#include "utils/log.h"

zlog_category_t *neuron = NULL;

static constexpr size_t kExpectedEdeTagCount = 1499;

static std::vector<std::string> split_semicolon(const std::string &line)
{
    std::vector<std::string> fields;
    std::string              current;

    for (char ch : line) {
        if (ch == ';') {
            fields.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    fields.push_back(current);

    return fields;
}

typedef struct {
    int             line_no;
    std::string     name;
    std::string     address;
    std::string     description;
    neu_attribute_e attribute;
} expected_addr_line_t;

static neu_attribute_e commandable_to_attribute(const std::string &commandable)
{
    if (commandable.empty()) {
        return NEU_ATTRIBUTE_READ;
    }

    if (commandable == "N" || commandable == "n" || commandable == "0" ||
        commandable == "false" || commandable == "FALSE") {
        return NEU_ATTRIBUTE_READ;
    }

    return (neu_attribute_e)(NEU_ATTRIBUTE_READ | NEU_ATTRIBUTE_WRITE);
}

static std::vector<expected_addr_line_t> expected_addresses_from_ede()
{
    static const std::unordered_map<int, std::string> object_area = {
        { 0, "AI" },   { 1, "AO" },   { 2, "AV" },   { 3, "BI" },
        { 4, "BO" },   { 5, "BV" },   { 8, "DEV" },  { 13, "MSI" },
        { 14, "MSO" }, { 19, "MSV" }, { 23, "ACC" },
    };

    std::vector<expected_addr_line_t> expected;
    std::ifstream                     ifs(EDE_TEST_FILE_PATH);
    std::string                       line;
    bool                              header_found = false;
    int                               line_no      = 0;

    while (std::getline(ifs, line)) {
        ++line_no;

        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (line.empty()) {
            continue;
        }

        if (line[0] == '#') {
            if (line.find("object-type") != std::string::npos &&
                line.find("object-instance") != std::string::npos) {
                header_found = true;
            }
            continue;
        }

        if (!header_found) {
            continue;
        }

        std::vector<std::string> fields = split_semicolon(line);
        if (fields.size() < 10) {
            continue;
        }

        int obj_type = -1;
        int obj_inst = -1;
        try {
            obj_type = std::stoi(fields[3]);
            obj_inst = std::stoi(fields[4]);
        } catch (...) {
            continue;
        }

        auto it = object_area.find(obj_type);
        if (it == object_area.end() || obj_inst < 0) {
            continue;
        }

        expected.emplace_back(expected_addr_line_t {
            line_no,
            fields[2],
            it->second + std::to_string(obj_inst),
            fields[5],
            commandable_to_attribute(fields[9]),
        });
    }

    return expected;
}

TEST(EdeTest, ParseEdeFileToAddressAndType)
{
    neu_ede_result_t result = { 0 };

    ASSERT_EQ(neu_ede_parse_file(EDE_TEST_FILE_PATH, &result), 0);
    ASSERT_EQ(result.count, kExpectedEdeTagCount);

    EXPECT_STREQ(result.entries[0].address, "DEV2");
    EXPECT_EQ(result.entries[0].data_type, NEU_TYPE_ERROR);
    EXPECT_STREQ(result.entries[0].name, "2");
    EXPECT_STREQ(result.entries[0].description, "ASP01");
    EXPECT_EQ(result.entries[0].attribute, NEU_ATTRIBUTE_READ);

    bool found_bv = false;
    for (size_t i = 0; i < result.count; ++i) {
        if (strcmp(result.entries[i].address, "BV2097192") == 0) {
            found_bv = true;
            EXPECT_EQ(result.entries[i].data_type, NEU_TYPE_BIT);
            break;
        }
    }
    EXPECT_TRUE(found_bv);

    neu_ede_result_uninit(&result);
}

TEST(EdeTest, FormatAddress)
{
    char address[128] = { 0 };

    EXPECT_EQ(neu_ede_format_address("AI", 0, NULL, false, 0, address,
                                     sizeof(address)),
              0);
    EXPECT_STREQ(address, "AI0");

    EXPECT_EQ(neu_ede_format_address("AI", 0, "Object_Name", false, 0, address,
                                     sizeof(address)),
              0);
    EXPECT_STREQ(address, "AI0.Object_Name");

    EXPECT_EQ(neu_ede_format_address("AI", 0, NULL, true, 1234, address,
                                     sizeof(address)),
              0);
    EXPECT_STREQ(address, "AI0.custom.1234");
}

TEST(EdeTest, NullAddressNotSupported)
{
    char address[128] = { 0 };

    EXPECT_NE(neu_ede_format_address("BO", 10, "NULL", false, 0, address,
                                     sizeof(address)),
              0);
}

TEST(EdeTest, DataTypeLookup)
{
    EXPECT_EQ(neu_ede_get_data_type("AI", NULL, false), NEU_TYPE_FLOAT);
    EXPECT_EQ(neu_ede_get_data_type("BO", NULL, false), NEU_TYPE_BIT);
    EXPECT_EQ(neu_ede_get_data_type("DEV", NULL, false), NEU_TYPE_ERROR);
    EXPECT_EQ(neu_ede_get_data_type("AI", "Object_Name", false),
              NEU_TYPE_STRING);
    EXPECT_EQ(neu_ede_get_data_type("AI", NULL, true), NEU_TYPE_CUSTOM);
    EXPECT_EQ(neu_ede_get_data_type("BO", "NULL", false), NEU_TYPE_ERROR);
}

TEST(EdeTest, ParseAllAddressesFromEde)
{
    neu_ede_result_t                  result = { 0 };
    std::vector<expected_addr_line_t> expected;

    ASSERT_EQ(neu_ede_parse_file(EDE_TEST_FILE_PATH, &result), 0);

    expected = expected_addresses_from_ede();
    ASSERT_EQ(result.count, kExpectedEdeTagCount);
    ASSERT_EQ(expected.size(), kExpectedEdeTagCount);

    for (size_t i = 0; i < expected.size(); ++i) {
        std::ostringstream trace;
        trace << "source line=" << expected[i].line_no
              << ", expected_name=" << expected[i].name
              << ", expected=" << expected[i].address
              << ", actual=" << result.entries[i].address
              << ", expected_attribute=" << expected[i].attribute
              << ", actual_attribute=" << result.entries[i].attribute;
        SCOPED_TRACE(trace.str());
        EXPECT_STREQ(result.entries[i].address, expected[i].address.c_str());
        EXPECT_STREQ(result.entries[i].description,
                     expected[i].description.c_str());
        EXPECT_STREQ(result.entries[i].name, expected[i].name.c_str());
        EXPECT_EQ(result.entries[i].attribute, expected[i].attribute);
    }

    neu_ede_result_uninit(&result);
}

TEST(EdeTest, ParseEdeFileToTags)
{
    neu_datatag_t *tags  = NULL;
    size_t         count = 0;

    ASSERT_EQ(neu_ede_parse_file_to_tags(EDE_TEST_FILE_PATH, &tags, &count), 0);
    ASSERT_NE(tags, nullptr);
    ASSERT_EQ(count, kExpectedEdeTagCount);

    EXPECT_STREQ(tags[0].name, "2");
    EXPECT_STREQ(tags[0].address, "DEV2");
    EXPECT_EQ(tags[0].attribute, NEU_ATTRIBUTE_READ);
    EXPECT_EQ(tags[0].type, NEU_TYPE_ERROR);

    bool found_bv = false;
    for (size_t i = 0; i < count; ++i) {
        if (strcmp(tags[i].address, "BV2097192") == 0) {
            found_bv = true;
            EXPECT_STREQ(tags[i].name, "BV-2097192");
            EXPECT_EQ(tags[i].attribute, NEU_ATTRIBUTE_READ);
            EXPECT_EQ(tags[i].type, NEU_TYPE_BIT);
            break;
        }
    }
    EXPECT_TRUE(found_bv);

    neu_ede_tags_uninit(tags, count);
}

TEST(EdeTest, EdeToMsg)
{
    neu_req_add_gtag_t cmd = { 0 };

    neu_ede_to_msg((char *) "bacnet-driver", EDE_TEST_FILE_PATH, &cmd);

    ASSERT_STREQ(cmd.driver, "bacnet-driver");
    ASSERT_EQ(cmd.n_group, 1);
    ASSERT_NE(cmd.groups, nullptr);
    ASSERT_STREQ(cmd.groups[0].group, "EDE_test");
    ASSERT_EQ(cmd.groups[0].interval, 1000);
    ASSERT_EQ(cmd.groups[0].n_tag, (int) kExpectedEdeTagCount);
    ASSERT_NE(cmd.groups[0].tags, nullptr);
    EXPECT_STREQ(cmd.groups[0].tags[0].address, "DEV2");

    neu_ede_tags_uninit(cmd.groups[0].tags, cmd.groups[0].n_tag);
    free(cmd.groups);
}

TEST(EdeTest, CommandableToRw)
{
    const char *  tmp_file = "/tmp/ede_commandable_test.csv";
    std::ofstream ofs(tmp_file);
    ofs << "PROJECT_NAME;X\n";
    ofs << "# keyname;device obj.-instance;object-name;object-type;"
           "object-instance;description;present-value-default;"
           "min-present-value;max-present-value;commandable\n";
    ofs << "k1;2;obj;1;10;desc;;;;Y\n";
    ofs << "k2;2;obj;1;11;desc;;;;N\n";
    ofs.close();

    neu_ede_result_t result = { 0 };
    ASSERT_EQ(neu_ede_parse_file(tmp_file, &result), 0);
    ASSERT_EQ(result.count, 2U);
    EXPECT_EQ(result.entries[0].attribute,
              (neu_attribute_e)(NEU_ATTRIBUTE_READ | NEU_ATTRIBUTE_WRITE));
    EXPECT_EQ(result.entries[1].attribute, NEU_ATTRIBUTE_READ);

    neu_ede_result_uninit(&result);
    std::remove(tmp_file);
}

int main(int argc, char **argv)
{
    zlog_init("./config/dev.conf");
    neuron = zlog_get_category("neuron");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}