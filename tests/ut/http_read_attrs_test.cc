#include <gtest/gtest.h>

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <vector>

extern "C" {
#include "msg.h"
#include "parser/neu_json_tag.h"
#include "restful/read_response.h"
#include "utils/log.h"
#include "json/neu_json_fn.h"

zlog_category_t *neuron = nullptr;
}

static_assert(offsetof(neu_resp_tag_info_t, tag) == 0,
              "GET-tag items must support configuration tag comparators");
static_assert(sizeof(neu_resp_tag_value_meta_t) ==
                  sizeof(neu_resp_tag_value_meta_paginate_t),
              "Read items must retain their common layout");
static_assert(offsetof(neu_resp_tag_value_meta_t, resolved_type) ==
                  offsetof(neu_resp_tag_value_meta_paginate_t, resolved_type),
              "Read resolved types must have a common offset");

using Json = std::unique_ptr<json_t, decltype(&json_decref)>;

static Json encode_sample(bool paginate, neu_tag_sample_attrs_t attrs,
                          bool error = false, bool legacy = false,
                          neu_type_e resolved_type   = NEU_TYPE_AUTO,
                          neu_type_e configured_type = NEU_TYPE_AUTO)
{
    UT_array *source;
    UT_icd    icd = { paginate ? sizeof(neu_resp_tag_value_meta_paginate_t)
                            : sizeof(neu_resp_tag_value_meta_t),
                   nullptr, nullptr, nullptr };
    utarray_new(source, &icd);
    neu_resp_tag_value_meta_t          tag           = {};
    neu_resp_tag_value_meta_paginate_t paginated_tag = {};
    tag.value.attrs = paginated_tag.value.attrs = attrs;
    tag.value.type                              = paginated_tag.value.type =
        error ? NEU_TYPE_ERROR : NEU_TYPE_DOUBLE;
    tag.resolved_type = paginated_tag.resolved_type = resolved_type;
    tag.datatag.type = paginated_tag.datatag.type = configured_type;
    if (paginate) {
        utarray_push_back(source, &paginated_tag);
    } else {
        utarray_push_back(source, &tag);
    }

    neu_json_tag_meta_t metas[3] = {};
    metas[0].name                = const_cast<char *>("timestamp");
    metas[0].t                   = NEU_JSON_INT;
    metas[0].value.val_int       = 11;
    metas[1].name                = const_cast<char *>("quality");
    metas[1].t                   = NEU_JSON_INT;
    metas[1].value.val_int       = 22;
    metas[2].name                = const_cast<char *>("resolved_type");
    metas[2].t                   = NEU_JSON_INT;
    metas[2].value.val_int       = NEU_TYPE_ERROR;

    neu_json_read_resp_tag_t          item      = {};
    neu_json_read_paginate_resp_tag_t page_item = {};
    item.name = page_item.name = const_cast<char *>("a");
    item.error = page_item.error = error ? 3002 : 0;
    item.t = page_item.t  = NEU_JSON_DOUBLE;
    item.value.val_double = page_item.value.val_double = 12.5;
    item.precision = page_item.precision = 2;
    item.n_meta = page_item.n_meta = legacy ? 3 : 0;
    item.datatag.type = page_item.datatag.type = configured_type;
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
            ASSERT_TRUE(
                json_is_integer(json_object_get(item, "resolved_type")));
            EXPECT_EQ(
                NEU_TYPE_AUTO,
                json_integer_value(json_object_get(item, "resolved_type")));
            if (paginate) {
                EXPECT_EQ(NEU_TYPE_ERROR,
                          json_integer_value(
                              json_object_get(legacy, "resolved_type")));
            }
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
        tag.value.type                         = NEU_TYPE_ERROR;
        tag.resolved_type =
            i == 0 ? NEU_TYPE_BOOL : i == 1 ? NEU_TYPE_AUTO : NEU_TYPE_INT16;
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
    for (int i = 0; i < 2; i++) {
        auto item = json_array_get(output, i);
        ASSERT_TRUE(json_is_integer(json_object_get(item, "resolved_type")));
        EXPECT_EQ(i == 0 ? NEU_TYPE_AUTO : NEU_TYPE_INT16,
                  json_integer_value(json_object_get(item, "resolved_type")));
        EXPECT_EQ(3002, json_integer_value(json_object_get(item, "error")));
    }
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

TEST(HttpReadResolvedType,
     RejectsNullOutputAndOffsetBoundsWithConfigurationCleanup)
{
    for (bool paginate : { false, true }) {
        UT_array *source;
        utarray_new(source,
                    paginate ? neu_resp_tag_value_meta_paginate_icd()
                             : neu_resp_tag_value_meta_icd());
        neu_resp_tag_value_meta_t          read_tag = {};
        neu_resp_tag_value_meta_paginate_t page_tag = {};
        for (int i = 0; i < 2; i++) {
            if (paginate) {
                utarray_push_back(source, &page_tag);
            } else {
                utarray_push_back(source, &read_tag);
            }
        }
        neu_json_read_resp_t          read_response = {};
        neu_json_read_paginate_resp_t page_response = {};
        read_response.n_tag = page_response.n_tag = 1;
        neu_http_read_response_t context          = {
            paginate ? static_cast<void *>(&page_response)
                     : static_cast<void *>(&read_response),
            source, 0, paginate
        };
        Json root(json_object(), json_decref);
        EXPECT_EQ(-1, neu_http_encode_read_response(root.get(), &context));
        for (unsigned int offset :
             { 1u, 2u, 3u, std::numeric_limits<unsigned int>::max() }) {
            neu_json_read_resp_tag_t          read_items[2] = {};
            neu_json_read_paginate_resp_tag_t page_items[2] = {};
            int                               count = offset == 1 ? 2 : 1;
            for (int i = 0; i < count && paginate; i++) {
                page_items[i].datatag.name        = strdup("a");
                page_items[i].datatag.address     = strdup("source");
                page_items[i].datatag.description = strdup("description");
                page_items[i].datatag.unit        = strdup("C");
            }
            read_response.n_tag = page_response.n_tag = count;
            read_response.tags                        = read_items;
            page_response.tags                        = page_items;
            context.offset                            = offset;
            EXPECT_EQ(-1, neu_http_encode_read_response(root.get(), &context));
        }
        utarray_free(source);
    }
}

TEST(HttpReadResolvedType, AuthoritativeTypeIsIndependentOfOutputAndErrors)
{
    const struct {
        neu_type_e configured;
        neu_type_e resolved;
    } cases[] = {
        { NEU_TYPE_AUTO, NEU_TYPE_AUTO },   { NEU_TYPE_AUTO, NEU_TYPE_INT16 },
        { NEU_TYPE_AUTO, NEU_TYPE_STRING }, { NEU_TYPE_AUTO, NEU_TYPE_BYTES },
        { NEU_TYPE_INT32, NEU_TYPE_INT32 }, { NEU_TYPE_BOOL, NEU_TYPE_BOOL },
    };
    for (bool paginate : { false, true }) {
        for (bool error : { false, true }) {
            for (const auto &test : cases) {
                SCOPED_TRACE(::testing::Message() << paginate << ":" << error
                                                  << ":" << test.resolved);
                auto root = encode_sample(paginate, {}, error, true,
                                          test.resolved, test.configured);
                auto item = first_item(root, paginate);
                auto type = json_object_get(item, "resolved_type");
                ASSERT_TRUE(json_is_integer(type));
                EXPECT_EQ(test.resolved, json_integer_value(type));
                if (paginate) {
                    EXPECT_EQ(
                        test.configured,
                        json_integer_value(json_object_get(item, "type")));
                    EXPECT_EQ(NEU_TYPE_ERROR,
                              json_integer_value(json_object_get(
                                  json_object_get(item, "attributes"),
                                  "resolved_type")));
                } else {
                    EXPECT_EQ(nullptr, json_object_get(item, "type"));
                }
                if (error) {
                    EXPECT_EQ(
                        3002,
                        json_integer_value(json_object_get(item, "error")));
                    EXPECT_EQ(nullptr, json_object_get(item, "value"));
                } else {
                    EXPECT_DOUBLE_EQ(
                        12.5,
                        json_number_value(json_object_get(item, "value")));
                }
            }
        }
    }
}

static void free_tags(UT_array *tags)
{
    utarray_free(tags);
}

using Tags = std::unique_ptr<UT_array, decltype(&free_tags)>;

static Tags new_tags(UT_icd *icd)
{
    UT_array *tags;
    utarray_new(tags, icd);
    return Tags(tags, free_tags);
}

static neu_json_tag_t configuration_tag(const neu_datatag_t &tag)
{
    neu_json_tag_t result = {};
    result.name           = tag.name;
    result.address        = tag.address;
    result.description    = tag.description;
    result.unit           = tag.unit;
    result.type           = tag.type;
    result.attribute      = tag.attribute;
    result.precision      = tag.precision;
    result.decimal        = tag.decimal;
    result.bias           = tag.bias;
    result.t              = NEU_JSON_UNDEFINE;
    return result;
}

TEST(HttpGetTagsResolvedType,
     SortedMixedItemsRetainConfigurationAndRuntimeTypes)
{
    auto             source       = new_tags(neu_resp_tag_info_icd());
    const char *     names[]      = { "pending", "scaled", "fixed", "bytes" };
    const char *     units[]      = { "C", "m", "V", "B" };
    const neu_type_e configured[] = { NEU_TYPE_AUTO, NEU_TYPE_AUTO,
                                      NEU_TYPE_BOOL, NEU_TYPE_AUTO };
    const neu_type_e resolved[]   = { NEU_TYPE_AUTO, NEU_TYPE_INT16,
                                    NEU_TYPE_BOOL, NEU_TYPE_BYTES };
    for (int i = 0; i < 4; i++) {
        neu_resp_tag_info_t item = {};
        item.tag.name            = const_cast<char *>(names[i]);
        item.tag.address         = const_cast<char *>(names[3 - i]);
        item.tag.description     = const_cast<char *>(names[i]);
        item.tag.unit            = const_cast<char *>(units[i]);
        item.tag.type            = configured[i];
        item.tag.attribute       = NEU_ATTRIBUTE_READ;
        item.tag.precision       = i;
        item.tag.decimal         = i * 0.5;
        item.tag.bias            = i;
        item.resolved_type       = resolved[i];
        utarray_push_back(source.get(), &item);
    }
    for (const char *sort : { "+name", "-name", "+type", "-type", "+address",
                              "-address", "+unit", "-unit" }) {
        SCOPED_TRACE(sort);
        auto cmp = neu_tag_get_sort_cmp(sort);
        ASSERT_NE(nullptr, cmp);
        utarray_sort(source.get(), cmp);
        std::vector<neu_json_tag_t> items;
        utarray_foreach(source.get(), neu_resp_tag_info_t *, item)
        {
            items.push_back(configuration_tag(item->tag));
        }
        neu_json_get_tags_resp_t response    = { static_cast<int>(items.size()),
                                              items.data() };
        neu_http_get_tags_response_t context = { &response, source.get() };
        char *                       encoded = nullptr;
        ASSERT_EQ(0,
                  neu_json_encode_by_fn(
                      &context, neu_http_encode_get_tags_response, &encoded));
        ASSERT_NE(nullptr, encoded);
        Json root(json_loads(encoded, JSON_REJECT_DUPLICATES, nullptr),
                  json_decref);
        free(encoded);
        ASSERT_NE(nullptr, root.get());
        Json exported(json_object(), json_decref);
        ASSERT_EQ(0, neu_json_encode_get_tags_resp(exported.get(), &response));
        auto output = json_object_get(root.get(), "tags");
        ASSERT_EQ(4u, json_array_size(output));
        for (unsigned int i = 0; i < 4; i++) {
            auto item     = json_array_get(output, i);
            auto original = static_cast<neu_resp_tag_info_t *>(
                utarray_eltptr(source.get(), i));
            const char *name = json_string_value(json_object_get(item, "name"));
            ASSERT_NE(nullptr, name);
            int index = 0;
            while (index < 4 && strcmp(names[index], name) != 0) {
                index++;
            }
            ASSERT_LT(index, 4);
            EXPECT_STREQ(original->tag.name, name);
            ASSERT_TRUE(
                json_is_integer(json_object_get(item, "resolved_type")));
            EXPECT_EQ(
                resolved[index],
                json_integer_value(json_object_get(item, "resolved_type")));
            EXPECT_EQ(configured[index],
                      json_integer_value(json_object_get(item, "type")));
            EXPECT_STREQ(units[index],
                         json_string_value(json_object_get(item, "unit")));
            auto config =
                json_array_get(json_object_get(exported.get(), "tags"), i);
            EXPECT_EQ(nullptr, json_object_get(config, "resolved_type"));
            EXPECT_EQ(configured[index],
                      json_integer_value(json_object_get(config, "type")));
            EXPECT_STREQ(units[index],
                         json_string_value(json_object_get(config, "unit")));
            if (i > 0) {
                EXPECT_LE(cmp(utarray_eltptr(source.get(), i - 1), original),
                          0);
            }
        }
    }
}

TEST(HttpGetTagsResolvedType, CombinedItemsDeepCopyAndMoveExportStrings)
{
    neu_resp_tag_info_t original = {};
    original.tag.name            = strdup("auto");
    original.tag.address         = strdup("source");
    original.tag.description     = strdup("description");
    original.tag.unit            = strdup("C");
    original.tag.type            = NEU_TYPE_AUTO;
    original.tag.decimal         = 0.5;
    original.resolved_type       = NEU_TYPE_INT16;
    auto source                  = new_tags(neu_resp_tag_info_icd());
    utarray_push_back(source.get(), &original);
    auto copy = static_cast<neu_resp_tag_info_t *>(utarray_front(source.get()));
    EXPECT_NE(original.tag.name, copy->tag.name);
    EXPECT_NE(original.tag.address, copy->tag.address);
    EXPECT_NE(original.tag.description, copy->tag.description);
    EXPECT_NE(original.tag.unit, copy->tag.unit);
    neu_tag_fini(&original.tag);

    auto second = new_tags(neu_resp_tag_info_icd());
    utarray_push_back(second.get(), copy);
    auto moved =
        static_cast<neu_resp_tag_info_t *>(utarray_front(second.get()));
    EXPECT_NE(copy->tag.name, moved->tag.name);
    EXPECT_NE(copy->tag.address, moved->tag.address);
    EXPECT_NE(copy->tag.description, moved->tag.description);
    EXPECT_NE(copy->tag.unit, moved->tag.unit);
    source.reset();
    EXPECT_EQ(NEU_TYPE_INT16, moved->resolved_type);
    EXPECT_EQ(NEU_TYPE_AUTO, moved->tag.type);
    EXPECT_DOUBLE_EQ(0.5, moved->tag.decimal);
    neu_json_tag_t config = configuration_tag(moved->tag);
    moved->tag.name = moved->tag.address = moved->tag.description =
        moved->tag.unit                  = nullptr;
    second.reset();
    EXPECT_STREQ("auto", config.name);
    EXPECT_STREQ("source", config.address);
    EXPECT_STREQ("description", config.description);
    EXPECT_STREQ("C", config.unit);
    Json root(json_object(), json_decref);
    EXPECT_EQ(0, neu_json_encode_tag(root.get(), &config));
    EXPECT_EQ(nullptr, json_object_get(root.get(), "resolved_type"));
    EXPECT_EQ(NEU_TYPE_AUTO,
              json_integer_value(json_object_get(root.get(), "type")));
    EXPECT_STREQ("C", json_string_value(json_object_get(root.get(), "unit")));
    neu_json_decode_tag_fini(&config);
}

TEST(HttpGetTagsResolvedType, EmptyItemsAndInvalidContextsAndBounds)
{
    Json root(json_object(), json_decref);
    EXPECT_EQ(-1, neu_http_encode_get_tags_response(root.get(), nullptr));
    neu_http_get_tags_response_t context = {};
    EXPECT_EQ(-1, neu_http_encode_get_tags_response(root.get(), &context));
    neu_json_get_tags_resp_t response = {};
    context.response                  = &response;
    EXPECT_EQ(0, neu_http_encode_get_tags_response(root.get(), &context));
    EXPECT_EQ(0u, json_array_size(json_object_get(root.get(), "tags")));
    response.n_tag = 1;
    EXPECT_EQ(-1, neu_http_encode_get_tags_response(root.get(), &context));
    response.n_tag = -1;
    EXPECT_EQ(-1, neu_http_encode_get_tags_response(root.get(), &context));
    response.n_tag             = 0;
    auto                source = new_tags(neu_resp_tag_info_icd());
    neu_resp_tag_info_t tag    = {};
    utarray_push_back(source.get(), &tag);
    context.tags = source.get();
    EXPECT_EQ(-1, neu_http_encode_get_tags_response(root.get(), &context));
    response.n_tag = 2;
    EXPECT_EQ(-1, neu_http_encode_get_tags_response(root.get(), &context));
    response.n_tag = 1;
    EXPECT_EQ(-1, neu_http_encode_get_tags_response(root.get(), &context));
    neu_json_tag_t item = {};
    response.tags       = &item;
    EXPECT_EQ(-1, neu_http_encode_get_tags_response(nullptr, &context));
    Json array(json_array(), json_decref);
    EXPECT_EQ(-1, neu_http_encode_get_tags_response(array.get(), &context));
    EXPECT_EQ(0, neu_http_encode_get_tags_response(root.get(), &context));
    EXPECT_TRUE(json_is_integer(
        json_object_get(json_array_get(json_object_get(root.get(), "tags"), 0),
                        "resolved_type")));
    EXPECT_EQ(1u, utarray_len(source.get()));
    EXPECT_NE(nullptr, neu_tag_get_sort_cmp("+name"));
    EXPECT_EQ(nullptr, neu_tag_get_sort_cmp("+resolved_type"));
}

static json_malloc_t saved_json_malloc;
static json_free_t   saved_json_free;
static size_t        json_allocation_count;
static size_t        json_fail_at;

static void *probe_json_malloc(size_t size)
{
    json_allocation_count++;
    return json_fail_at == json_allocation_count ? nullptr
                                                 : saved_json_malloc(size);
}

class JsonAllocationScope {
public:
    explicit JsonAllocationScope(size_t fail_at = 0)
    {
        json_get_alloc_funcs(&saved_json_malloc, &saved_json_free);
        json_allocation_count = 0;
        json_fail_at          = fail_at;
        json_set_alloc_funcs(probe_json_malloc, saved_json_free);
    }
    ~JsonAllocationScope()
    {
        json_set_alloc_funcs(saved_json_malloc, saved_json_free);
    }
};

TEST(HttpResolvedType, InsertionFailureReleasesDomAndPreservesSourceOwnership)
{
    for (int mode = 0; mode < 3; mode++) {
        SCOPED_TRACE(mode);
        auto source = new_tags(mode == 2 ? neu_resp_tag_info_icd()
                                         : mode == 1
                                       ? neu_resp_tag_value_meta_paginate_icd()
                                       : neu_resp_tag_value_meta_icd());
        neu_resp_tag_info_t get_tag                 = {};
        get_tag.tag.name                            = const_cast<char *>("a");
        get_tag.resolved_type                       = NEU_TYPE_INT16;
        neu_resp_tag_value_meta_t read_tag          = {};
        read_tag.resolved_type                      = NEU_TYPE_INT16;
        neu_resp_tag_value_meta_paginate_t page_tag = {};
        page_tag.resolved_type                      = NEU_TYPE_INT16;
        const void *tag                             = mode == 2
            ? static_cast<void *>(&get_tag)
            : mode == 1 ? static_cast<void *>(&page_tag)
                        : static_cast<void *>(&read_tag);
        utarray_push_back(source.get(), tag);
        neu_json_tag_t               get_item = configuration_tag(get_tag.tag);
        neu_json_get_tags_resp_t     get_response       = { 1, &get_item };
        neu_http_get_tags_response_t get_context        = { &get_response,
                                                     source.get() };
        neu_json_read_resp_tag_t     read_item          = {};
        neu_json_read_resp_t         read_response      = { 1, &read_item };
        neu_json_read_paginate_resp_tag_t page_item     = {};
        neu_json_read_paginate_resp_t     page_response = {};
        page_response.n_tag                             = 1;
        page_response.tags                              = &page_item;
        neu_http_read_response_t read_context           = {
            mode == 1 ? static_cast<void *>(&page_response)
                      : static_cast<void *>(&read_response),
            source.get(), 0, mode == 1
        };
        auto encode = mode == 2 ? neu_http_encode_get_tags_response
                                : neu_http_encode_read_response;
        void *context = mode == 2 ? static_cast<void *>(&get_context)
                                  : static_cast<void *>(&read_context);
        Json   baseline(json_object(), json_decref);
        size_t allocations = 0;
        {
            JsonAllocationScope probe;
            EXPECT_EQ(0, encode(baseline.get(), context));
            allocations = json_allocation_count;
        }
        ASSERT_GT(allocations, 0u);
        Json failed(json_object(), json_decref);
        {
            JsonAllocationScope probe(allocations);
            EXPECT_EQ(-1, encode(failed.get(), context));
        }
        EXPECT_EQ(1u, utarray_len(source.get()));
        if (mode == 2) {
            Json failed_array(json_object(), json_decref);
            {
                JsonAllocationScope probe(1);
                EXPECT_EQ(-1, encode(failed_array.get(), context));
            }
            auto item =
                static_cast<neu_resp_tag_info_t *>(utarray_front(source.get()));
            EXPECT_STREQ("a", item->tag.name);
            EXPECT_EQ(NEU_TYPE_INT16, item->resolved_type);
        }
    }
}
