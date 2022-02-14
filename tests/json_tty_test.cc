#include <stdlib.h>

#include <gtest/gtest.h>

#include "json/neu_json_fn.h"
#include "json/neu_json_tty.h"

TEST(JsonTtytest, GetTtyEncode)
{
    char *buf =
        (char *) "{\"ttys\": [\"/dev/tty0\", \"/dev/tty1\", \"/dev/tty2\", "
                 "\"/dev/tty3\", \"/dev/tty4\", \"/dev/tty5\", \"/dev/tty6\"]}";

    char *result = NULL;

    neu_json_get_tty_resp_t resp = {
        .n_tty = 7,
    };

    resp.ttys = (neu_json_get_tty_resp_tty_t *) calloc(
        7, sizeof(neu_json_get_tty_resp_tty_t));

    resp.ttys[0] = strdup((char *) "/dev/tty0");
    resp.ttys[1] = strdup((char *) "/dev/tty1");
    resp.ttys[2] = strdup((char *) "/dev/tty2");
    resp.ttys[3] = strdup((char *) "/dev/tty3");
    resp.ttys[4] = strdup((char *) "/dev/tty4");
    resp.ttys[5] = strdup((char *) "/dev/tty5");
    resp.ttys[6] = strdup((char *) "/dev/tty6");

    EXPECT_EQ(
        0, neu_json_encode_by_fn(&resp, neu_json_encode_get_tty_resp, &result));
    EXPECT_STREQ(buf, result);

    free(resp.ttys[0]);
    free(resp.ttys[1]);
    free(resp.ttys[2]);
    free(resp.ttys[3]);
    free(resp.ttys[4]);
    free(resp.ttys[5]);
    free(resp.ttys[6]);
    free(resp.ttys);

    free(result);
}