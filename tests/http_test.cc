#include <gtest/gtest.h>

#include "adapter.h"
#include "utils/http.h"
#include <nng/nng.h>
#include <nng/supplemental/http/http.h>

#include "utils/log.h"

zlog_category_t *neuron = NULL;
#define INVAL ((size_t) -1)

typedef struct {
    const char *uri;
    const char *name;
    const char *val;
    size_t      len;
} query_param_data_t;

typedef struct {
    const char *uri;
    size_t      node_id;
    int         error;
} query_node_id_data_t;

typedef struct {
    const char *    uri;
    neu_node_type_e node_type;
    int             error;
} query_node_type_data_t;

TEST(HTTPTest, http_get_param)
{
    nng_aio *                aio    = NULL;
    nng_http_req *           req    = NULL;
    nng_url *                url    = NULL;
    size_t                   len    = 0;
    const query_param_data_t data[] = {
        { .uri = "/?node_id=1", .name = "xxxx", .val = NULL, .len = 0 },
        { .uri = "/?node_id=1", .name = "node", .val = NULL, .len = 0 },
        { .uri = "/?node_id=1", .name = "node=", .val = NULL, .len = 0 },
        { .uri = "/?node_id=1", .name = "node&", .val = NULL, .len = 0 },
        { .uri = "/?node_id=1", .name = "node_id=", .val = NULL, .len = 0 },
        { .uri = "/?node_id=1", .name = "id", .val = NULL, .len = 0 },

        { .uri = "/?node_id", .name = "node_id", .val = "", .len = 0 },
        { .uri = "/?node_id", .name = "id", .val = NULL, .len = 0 },
        { .uri = "/?node_id=", .name = "node_id", .val = "", .len = 0 },
        { .uri = "/?node_id=", .name = "id", .val = NULL, .len = 0 },
        { .uri = "/?node_id#", .name = "node_id", .val = "#", .len = 0 },
        { .uri = "/?node_id#", .name = "id", .val = NULL, .len = 0 },

        { .uri = "/?node_id=1", .name = "node_id", .val = "1", .len = 1 },
        { .uri = "/?node_id=1&", .name = "node_id", .val = "1", .len = 1 },
        { .uri = "/?node_id=1&", .name = "id", .val = NULL, .len = 0 },
        { .uri  = "/?node_id=1&type=1",
          .name = "node_id",
          .val  = "1",
          .len  = 1 },
    };

    nng_aio_alloc(&aio, NULL, NULL);
    nng_url_parse(&url, "http://127.0.0.1");
    nng_http_req_alloc(&req, url);

    for (uint64_t i = 0; i < sizeof(data) / sizeof(query_param_data_t); ++i) {
        nng_http_req_set_uri(req, (char *) data[i].uri);
        nng_aio_set_input(aio, 0, req);
        len             = 0;
        const char *val = neu_http_get_param(aio, data[i].name, &len);
        EXPECT_EQ(len, data[i].len);
        if (val && data[i].val) {
            EXPECT_EQ(strncmp(val, data[i].val, len), 0);
        } else {
            EXPECT_EQ(val, data[i].val);
        }
    }

    nng_aio_free(aio);
    nng_http_req_free(req);
    nng_url_free(url);
}

TEST(HTTPTest, http_get_param_str)
{
    nng_aio *     aio = NULL;
    nng_http_req *req = NULL;
    nng_url *     url = NULL;
    ssize_t       ret = 0;

    char buf[256] = { 0 };
    nng_aio_alloc(&aio, NULL, NULL);
    nng_url_parse(&url, "http://127.0.0.1");
    nng_http_req_alloc(&req, url);
    nng_aio_set_input(aio, 0, req);

    nng_http_req_set_uri(req, "/?node_id");
    ret = neu_http_get_param_str(aio, "group", buf, sizeof(buf));
    EXPECT_EQ(ret, -2);

    nng_http_req_set_uri(req, "/?group");
    ret = neu_http_get_param_str(aio, "group", buf, sizeof(buf));
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(0, strcmp("", buf));

    nng_http_req_set_uri(req, "/?group=%EZ%BB%84");
    ret = neu_http_get_param_str(aio, "group", buf, 3);
    EXPECT_EQ(ret, -1);
    EXPECT_EQ(0, strcmp("", buf));

    nng_http_req_set_uri(req, "/?group=%E7%BB%84");
    ret = neu_http_get_param_str(aio, "group", buf, 4);
    EXPECT_EQ(ret, 3);
    EXPECT_EQ(0, strcmp("组", buf));

    nng_http_req_set_uri(req, "/?group=%E7%BB%84");
    ret = neu_http_get_param_str(aio, "group", buf, 3);
    EXPECT_EQ(ret, 3);
    EXPECT_EQ(0, strncmp("组", buf, 2));

    nng_aio_free(aio);
    nng_http_req_free(req);
    nng_url_free(url);
}

int main(int argc, char **argv)
{
    zlog_init("./config/dev.conf");
    neuron = zlog_get_category("neuron");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}