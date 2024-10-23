#include <gtest/gtest.h>

#include "neuron.h"
#include "utils/cid.h"

zlog_category_t *neuron = NULL;
TEST(cid_parse, cid)
{
    cid_t cid = { 0 };
    int   ret = neu_cid_parse("./config/serverBMS_3_test.cid", &cid);
    EXPECT_EQ(ret, 0);
    neu_cid_free(&cid);
}

TEST(cid_to_msg, cid)
{
    cid_t cid = { 0 };
    int   ret = neu_cid_parse("./config/serverBMS_3_test.cid", &cid);
    EXPECT_EQ(ret, 0);

    const char *driver = "driver";

    neu_req_add_gtag_t cmd = { 0 };
    neu_cid_to_msg((char *) driver, &cid, &cmd);
    neu_cid_free(&cid);

    EXPECT_STREQ(cmd.driver, "driver");
    EXPECT_EQ(cmd.n_group, 12);

    for (int i = 0; i < cmd.n_group; i++) {
        for (int j = 0; j < cmd.groups[i].n_tag; j++) {
            if (cmd.groups[i].tags[j].name != NULL) {
                free(cmd.groups[i].tags[j].name);
            }
            if (cmd.groups[i].tags[j].address != NULL) {
                free(cmd.groups[i].tags[j].address);
            }
            if (cmd.groups[i].tags[j].description != NULL) {
                free(cmd.groups[i].tags[j].description);
            }
        }
        free(cmd.groups[i].tags);
        free(cmd.groups[i].context);
    }
    free(cmd.groups);
}

TEST(cid_string, cid)
{
    cid_dataset_info_t info = { 0 };
    info.control            = true;
    strcpy(info.ied_name, "ied_name");
    strcpy(info.ldevice_inst, "ldevice_inst");
    strcpy(info.ln_class, "ln_class");
    info.buffered = true;
    strcpy(info.report_name, "report_name");
    strcpy(info.report_id, "report_id");
    strcpy(info.dataset_name, "dataset_name");

    char *str = neu_cid_info_to_string(&info);
    EXPECT_STREQ(
        str,
        "{\"control\": true, \"buffered\": true, \"ied_name\": \"ied_name\", "
        "\"ldevice_inst\": \"ldevice_inst\", \"ln_class\": "
        "\"ln_class\", \"report_name\": \"report_name\", "
        "\"report_id\": \"report_id\", \"dataset_name\": \"dataset_name\"}");
    free(str);
}

TEST(cid_from_string, cid)
{
    cid_dataset_info_t *info = NULL;
    const char *        str =
        "{\"control\": true, \"buffered\": true, \"ied_name\": \"ied_name\", "
        "\"ldevice_inst\": \"ldevice_inst\", \"ln_class\": "
        "\"ln_class\", \"report_name\": \"report_name\", "
        "\"report_id\": \"report_id\", \"dataset_name\": \"dataset_name\"}";

    info = neu_cid_info_from_string(str);
    EXPECT_EQ(info->control, true);
    EXPECT_STREQ(info->ied_name, "ied_name");
    EXPECT_STREQ(info->ldevice_inst, "ldevice_inst");
    EXPECT_STREQ(info->ln_class, "ln_class");
    EXPECT_EQ(info->buffered, true);
    EXPECT_STREQ(info->report_name, "report_name");
    EXPECT_STREQ(info->report_id, "report_id");
    EXPECT_STREQ(info->dataset_name, "dataset_name");
    free(info);
}

int main(int argc, char **argv)
{
    zlog_init("./config/dev.conf");
    neuron = zlog_get_category("neuron");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
