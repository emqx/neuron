#include <gtest/gtest.h>

#include <cstring>
#include <limits>
#include <memory>

extern "C" {
#include "msg.h"
#include "restful/read_response.h"
#include "utils/log.h"

zlog_category_t *neuron = nullptr;
}

using Json = std::unique_ptr<json_t, decltype(&json_decref)>;

static Json encode_sample(bool paginate, neu_tag_sample_attrs_t attrs,
                          bool error = false, bool legacy = false)
{
    UT_array *source;
    UT_icd    icd = { paginate ? sizeof(neu_resp_tag_value_meta_paginate_t)
                            : sizeof(neu_resp_tag_value_meta_t),
                   nullptr, nullptr, nullptr };
    utarray_new(source, &icd);
    neu_resp_tag_value_meta_t          tag           = {};
    neu_resp_tag_value_meta_paginate_t paginated_tag = {};
    tag.value.attrs = paginated_tag.value.attrs = attrs;
    if (paginate) {
        utarray_push_back(source, &paginated_tag);
    } else {
        utarray_push_back(source, &tag);
    }

    neu_json_tag_meta_t metas[2] = {};
    metas[0].name                = const_cast<char *>("timestamp");
    metas[0].t                   = NEU_JSON_INT;
    metas[0].value.val_int       = 11;
    metas[1].name                = const_cast<char *>("quality");
    metas[1].t                   = NEU_JSON_INT;
    metas[1].value.val_int       = 22;

    neu_json_read_resp_tag_t          item      = {};
    neu_json_read_paginate_resp_tag_t page_item = {};
    item.name = page_item.name = const_cast<char *>("a");
    item.error = page_item.error = error ? 3002 : 0;
    item.t = page_item.t  = NEU_JSON_DOUBLE;
    item.value.val_double = page_item.value.val_double = 12.5;
    item.precision = page_item.precision = 2;
    item.n_meta = page_item.n_meta = legacy ? 2 : 0;
    item.metas = page_item.metas           = metas;
    neu_json_read_resp_t          response = { 1, &item };
    neu_json_read_paginate_resp_t page     = {};
    page.n_tag                             = 1;
    page.tags                              = &page_item;
    page.meta.current_page                 = 2;
    page.meta.page_size                    = 1;
    page.meta.total                        = 3;
    neu_http_read_response_t context       = {};
    context.response = paginate ? (void *) &page : (void *) &response;
    context.tags     = source;
    context.paginate = paginate;
    Json root(json_object(), json_decref);
    EXPECT_EQ(0, neu_http_encode_read_response(root.get(), &context));
    utarray_free(source);
    return root;
}

static json_t *first_item(const Json &root, bool paginate)
{
    return json_array_get(
        json_object_get(root.get(), paginate ? "items" : "tags"), 0);
}

TEST(HttpReadAttrs, IndependentOptionalFields)
{
    const char *names[] = { "timestamp", "quality", "source_timestamp",
                            "server_timestamp" };
    for (bool paginate : { false, true }) {
        for (int flags = 0; flags < 16; ++flags) {
            SCOPED_TRACE(::testing::Message() << paginate << ":" << flags);
            neu_tag_sample_attrs_t attrs = {};
            attrs.timestamp              = std::numeric_limits<int64_t>::min();
            attrs.quality                = std::numeric_limits<int32_t>::max();
            attrs.source_timestamp       = 0;
            attrs.server_timestamp       = std::numeric_limits<int64_t>::max();
            attrs.has_timestamp          = flags & 1;
            attrs.has_quality            = flags & 2;
            attrs.has_source_timestamp   = flags & 4;
            attrs.has_server_timestamp   = flags & 8;
            auto root                    = encode_sample(paginate, attrs);
            auto item                    = first_item(root, paginate);
            ASSERT_TRUE(json_is_object(item));
            const int64_t expected[] = { attrs.timestamp, attrs.quality,
                                         attrs.source_timestamp,
                                         attrs.server_timestamp };
            for (int i = 0; i < 4; ++i) {
                auto field = json_object_get(item, names[i]);
                if (flags & (1 << i)) {
                    ASSERT_TRUE(json_is_integer(field));
                    EXPECT_EQ(expected[i], json_integer_value(field));
                } else {
                    EXPECT_EQ(nullptr, field);
                }
            }
            EXPECT_DOUBLE_EQ(12.5,
                             json_number_value(json_object_get(item, "value")));
            EXPECT_EQ(
                2,
                json_integer_value(json_object_get(item, "transferPrecision")));
            EXPECT_EQ(nullptr, json_object_get(item, "error"));
            if (paginate) {
                EXPECT_TRUE(
                    json_is_object(json_object_get(item, "attributes")));
                EXPECT_EQ(3,
                          json_integer_value(json_object_get(
                              json_object_get(root.get(), "meta"), "total")));
            }
        }
    }
}

TEST(HttpReadAttrs, ExplicitZerosAndErrorSamples)
{
    neu_tag_sample_attrs_t attrs = {};
    attrs.has_timestamp = attrs.has_quality = true;
    attrs.has_source_timestamp = attrs.has_server_timestamp = true;
    for (bool paginate : { false, true }) {
        auto root = encode_sample(paginate, attrs, true);
        auto item = first_item(root, paginate);
        for (const char *name : { "timestamp", "quality", "source_timestamp",
                                  "server_timestamp" }) {
            ASSERT_TRUE(json_is_integer(json_object_get(item, name)));
            EXPECT_EQ(0, json_integer_value(json_object_get(item, name)));
        }
        EXPECT_EQ(3002, json_integer_value(json_object_get(item, "error")));
        EXPECT_EQ(nullptr, json_object_get(item, "value"));
        EXPECT_EQ(nullptr, json_object_get(item, "transferPrecision"));
    }
}

TEST(HttpReadAttrs, LegacyMetadataCollisionAndNesting)
{
    for (bool paginate : { false, true }) {
        for (bool present : { false, true }) {
            neu_tag_sample_attrs_t attrs = {};
            attrs.has_timestamp = attrs.has_quality = present;
            attrs.timestamp                         = 100;
            attrs.quality                           = 192;
            auto root   = encode_sample(paginate, attrs, false, true);
            auto item   = first_item(root, paginate);
            auto legacy = paginate ? json_object_get(item, "attributes") : item;
            EXPECT_EQ(paginate || !present ? 11 : 100,
                      json_integer_value(json_object_get(legacy, "timestamp")));
            EXPECT_EQ(paginate || !present ? 22 : 192,
                      json_integer_value(json_object_get(legacy, "quality")));
            if (paginate) {
                EXPECT_EQ(present,
                          json_object_get(item, "timestamp") != nullptr);
                if (present) {
                    EXPECT_EQ(
                        100,
                        json_integer_value(json_object_get(item, "timestamp")));
                }
            }
            char *encoded = json_dumps(root.get(), 0);
            Json  checked(json_loads(encoded, JSON_REJECT_DUPLICATES, nullptr),
                         json_decref);
            free(encoded);
            EXPECT_NE(nullptr, checked.get());
        }
    }
}

TEST(HttpReadAttrs, FilteredPageUsesSourceOffset)
{
    UT_array *source;
    utarray_new(source, neu_resp_tag_value_meta_paginate_icd());
    for (int i = 0; i < 3; ++i) {
        neu_resp_tag_value_meta_paginate_t tag = {};
        tag.value.attrs.has_quality            = true;
        tag.value.attrs.quality                = 100 + i;
        utarray_push_back(source, &tag);
    }
    neu_json_read_paginate_resp_tag_t items[2] = {};
    items[0].name                              = const_cast<char *>("b");
    items[1].name                              = const_cast<char *>("c");
    for (auto &item : items) {
        item.error = 3002;
    }
    neu_json_read_paginate_resp_t response = {};
    response.n_tag                         = 2;
    response.tags                          = items;
    neu_http_read_response_t context       = { &response, source, 1, true };
    Json                     root(json_object(), json_decref);
    ASSERT_EQ(0, neu_http_encode_read_response(root.get(), &context));
    auto output = json_object_get(root.get(), "items");
    EXPECT_EQ(101,
              json_integer_value(
                  json_object_get(json_array_get(output, 0), "quality")));
    EXPECT_EQ(102,
              json_integer_value(
                  json_object_get(json_array_get(output, 1), "quality")));
    utarray_free(source);
}

TEST(HttpReadAttrs, RejectsInvalidContextsAndReleasesPageConfiguration)
{
    Json root(json_object(), json_decref);
    EXPECT_EQ(-1, neu_http_encode_read_response(root.get(), nullptr));
    neu_http_read_response_t empty = {};
    EXPECT_EQ(-1, neu_http_encode_read_response(root.get(), &empty));

    for (bool invalid_object : { false, true }) {
        UT_array *source;
        utarray_new(source, neu_resp_tag_value_meta_paginate_icd());
        if (invalid_object) {
            neu_resp_tag_value_meta_paginate_t tag = {};
            utarray_push_back(source, &tag);
        }
        neu_json_read_paginate_resp_tag_t item = {};
        item.datatag.name                      = strdup("a");
        item.datatag.address                   = strdup("sample");
        item.datatag.description               = strdup("");
        item.datatag.unit                      = strdup("C");
        neu_json_read_paginate_resp_t response = {};
        response.n_tag                         = 1;
        response.tags                          = &item;
        neu_http_read_response_t context       = { &response, source, 0, true };
        EXPECT_EQ(-1,
                  neu_http_encode_read_response(
                      invalid_object ? nullptr : root.get(), &context));
        utarray_free(source);
    }
}

TEST(HttpReadAttrs, EmptyPagesAndInvalidSourceBounds)
{
    for (bool paginate : { false, true }) {
        neu_json_read_resp_t          response = {};
        neu_json_read_paginate_resp_t page     = {};
        neu_http_read_response_t      context  = {};
        context.response = paginate ? (void *) &page : (void *) &response;
        context.paginate = paginate;
        context.offset   = std::numeric_limits<unsigned int>::max();
        Json root(json_object(), json_decref);
        EXPECT_EQ(0, neu_http_encode_read_response(root.get(), &context));
        auto items = json_object_get(root.get(), paginate ? "items" : "tags");
        EXPECT_TRUE(json_is_array(items));
        EXPECT_EQ(0u, json_array_size(items));
        response.n_tag = page.n_tag = 1;
        EXPECT_EQ(-1, neu_http_encode_read_response(root.get(), &context));
        response.n_tag = page.n_tag = -1;
        EXPECT_EQ(-1, neu_http_encode_read_response(root.get(), &context));
    }
}
