#include <gtest/gtest.h>

#include "adapter.h"
#include "http.h"
#include <nng/nng.h>
#include <nng/supplemental/http/http.h>

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

        { .uri = "/?node_id", .name = "node_id", .val = "", .len = 0 },
        { .uri = "/?node_id=", .name = "node_id", .val = "", .len = 0 },
        { .uri = "/?node_id#", .name = "node_id", .val = "#", .len = 0 },

        { .uri = "/?node_id=1", .name = "node_id", .val = "1", .len = 1 },
        { .uri = "/?node_id=1&", .name = "node_id", .val = "1", .len = 1 },
        { .uri  = "/?node_id=1&type=1",
          .name = "node_id",
          .val  = "1",
          .len  = 1 },
    };

    nng_aio_alloc(&aio, NULL, NULL);
    nng_url_parse(&url, "http://127.0.0.1");
    nng_http_req_alloc(&req, url);

    for (int i = 0; i < sizeof(data) / sizeof(query_param_data_t); ++i) {
        nng_http_req_set_uri(req, (char *) data[i].uri);
        nng_aio_set_input(aio, 0, req);
        len             = 0;
        const char *val = http_get_param(aio, data[i].name, &len);
        EXPECT_EQ(len, data[i].len);
        EXPECT_EQ(strncmp(val, data[i].val, len), 0);
    }

    nng_aio_free(aio);
    nng_http_req_free(req);
    nng_url_free(url);
}

TEST(HTTPTest, http_get_param_node_id)
{
    nng_aio *     aio     = NULL;
    nng_http_req *req     = NULL;
    nng_url *     url     = NULL;
    neu_node_id_t node_id = { 0 };
    int           rv      = 0;

    const query_node_id_data_t data[] = {
        { .uri = "/?node_id=1", .node_id = 1, .error = 0 },
        { .uri = "/?node_id=123", .node_id = 123, .error = 0 },
        { .uri = "/?node_id=456&", .node_id = 456, .error = 0 },
        { .uri = "/?node_id=1&type=1", .node_id = 1, .error = 0 },
        { .uri = "/?hello=world", .node_id = INVAL, .error = NEU_ERR_ENOENT },
        { .uri = "/?node_id=0", .node_id = INVAL, .error = NEU_ERR_EINVAL },
        { .uri = "/?node_id", .node_id = INVAL, .error = NEU_ERR_EINVAL },
        { .uri = "/?node_id=xx", .node_id = INVAL, .error = NEU_ERR_EINVAL },
        { .uri     = "/?node_id=99999999999999999999999999999999999999999",
          .node_id = 1,
          .error   = NEU_ERR_EINVAL },
    };

    nng_aio_alloc(&aio, NULL, NULL);
    nng_url_parse(&url, "http://127.0.0.1");
    nng_http_req_alloc(&req, url);

    for (int i = 0; i < sizeof(data) / sizeof(query_node_id_data_t); ++i) {
        neu_node_id_t prev_node_id = node_id;
        nng_http_req_set_uri(req, (char *) data[i].uri);
        nng_aio_set_input(aio, 0, req);
        rv = http_get_param_node_id(aio, "node_id", &node_id);
        EXPECT_EQ(rv, data[i].error);
        if (INVAL != data[i].node_id) {
            EXPECT_EQ(node_id, data[i].node_id);
        } else {
            EXPECT_EQ(node_id, prev_node_id);
        }
    }

    nng_aio_free(aio);
    nng_http_req_free(req);
    nng_url_free(url);
}

TEST(HTTPTest, http_get_param_node_type)
{
    nng_aio *       aio       = NULL;
    nng_http_req *  req       = NULL;
    nng_url *       url       = NULL;
    neu_node_type_e node_type = NEU_NODE_TYPE_UNKNOW;
    int             rv        = 0;
    char            uri[256]  = { 0 };

    nng_aio_alloc(&aio, NULL, NULL);
    nng_url_parse(&url, "http://127.0.0.1");
    nng_http_req_alloc(&req, url);

    nng_http_req_set_uri(req, "/?node_id=1");
    nng_aio_set_input(aio, 0, req);
    rv = http_get_param_node_type(aio, "type", &node_type);
    EXPECT_EQ(rv, NEU_ERR_ENOENT);

    nng_http_req_set_uri(req, "/?type");
    nng_aio_set_input(aio, 0, req);
    rv = http_get_param_node_type(aio, "type", &node_type);
    EXPECT_EQ(rv, NEU_ERR_EINVAL);

    nng_http_req_set_uri(req, "/?type=12345");
    nng_aio_set_input(aio, 0, req);
    rv = http_get_param_node_type(aio, "type", &node_type);
    EXPECT_EQ(rv, NEU_ERR_EINVAL);

    snprintf(uri, sizeof(uri), "/?type=%d", NEU_NODE_TYPE_MAX);
    nng_http_req_set_uri(req, uri);
    nng_aio_set_input(aio, 0, req);
    rv = http_get_param_node_type(aio, "type", &node_type);
    EXPECT_EQ(rv, NEU_ERR_EINVAL);

    for (int i = NEU_NODE_TYPE_UNKNOW; i < NEU_NODE_TYPE_MAX; ++i) {
        snprintf(uri, sizeof(uri), "/?type=%d", i);
        nng_http_req_set_uri(req, uri);
        nng_aio_set_input(aio, 0, req);
        rv = http_get_param_node_type(aio, "type", &node_type);
        EXPECT_EQ(rv, 0);
        EXPECT_EQ(node_type, (neu_node_type_e) i);
    }

    nng_aio_free(aio);
    nng_http_req_free(req);
    nng_url_free(url);
}
