#include <stdio.h>
#include <stdlib.h>

#include <gtest/gtest.h>

#include "tag_sort.h"

extern "C" {
#include "base/group.h"
#include "json/neu_json_rw.h"
}

#include "utils/log.h"

zlog_category_t *neuron = NULL;
struct tag {
    uint8_t  station;
    uint8_t  area;
    uint16_t address;
    uint8_t  type;
};

static int tag_cmp(neu_tag_sort_elem_t *tag1, neu_tag_sort_elem_t *tag2)
{
    int ret = memcmp(tag1->tag, tag2->tag, sizeof(struct tag));

    return ret;
};

static bool tag_sort_fn(neu_tag_sort_t *sort, void *tag, void *tag_to_be_sorted)
{
    struct tag *p_tag  = (struct tag *) tag;
    struct tag *p_tag1 = (struct tag *) tag_to_be_sorted;

    if (p_tag->station != p_tag1->station) {
        return false;
    }

    if (p_tag->area != p_tag1->area) {
        return false;
    }

    if (p_tag->address + 1 < p_tag1->address) {
        return false;
    }

    return true;
}

TEST(TagSortTest, SortSimple)
{
    UT_array *             tags   = NULL;
    neu_tag_sort_result_t *result = NULL;
    struct tag *           tag1 = (struct tag *) calloc(1, sizeof(struct tag));
    struct tag *           tag2 = (struct tag *) calloc(1, sizeof(struct tag));
    struct tag *           tag3 = (struct tag *) calloc(1, sizeof(struct tag));

    tag1->type    = 1;
    tag1->station = 1;
    tag1->area    = 1;
    tag1->address = 1;

    tag2->type    = 1;
    tag2->station = 1;
    tag2->area    = 1;
    tag2->address = 2;

    tag3->type    = 1;
    tag3->station = 1;
    tag3->area    = 1;
    tag3->address = 3;

    utarray_new(tags, &ut_ptr_icd);
    utarray_push_back(tags, &tag2);
    utarray_push_back(tags, &tag1);
    utarray_push_back(tags, &tag3);

    result = neu_tag_sort(tags, tag_sort_fn, tag_cmp);

    EXPECT_EQ(1, result->n_sort);
    EXPECT_EQ(result->sorts->info.size, 3);

    struct tag **p_tag =
        (struct tag **) utarray_eltptr(result->sorts[0].tags, 0);
    EXPECT_EQ((*p_tag)->address, 1);

    p_tag = (struct tag **) utarray_eltptr(result->sorts[0].tags, 1);
    EXPECT_EQ((*p_tag)->address, 2);

    p_tag = (struct tag **) utarray_eltptr(result->sorts[0].tags, 2);
    EXPECT_EQ((*p_tag)->address, 3);

    neu_tag_sort_free(result);
    utarray_free(tags);

    free(tag1);
    free(tag2);
    free(tag3);
}

TEST(TagSortTest, SortDup)
{
    UT_array *             tags   = NULL;
    neu_tag_sort_result_t *result = NULL;
    struct tag *           tag1 = (struct tag *) calloc(1, sizeof(struct tag));
    struct tag *           tag2 = (struct tag *) calloc(1, sizeof(struct tag));
    struct tag *           tag3 = (struct tag *) calloc(1, sizeof(struct tag));
    struct tag *           tag4 = (struct tag *) calloc(1, sizeof(struct tag));

    tag1->type    = 1;
    tag1->station = 1;
    tag1->area    = 1;
    tag1->address = 1;

    tag2->type    = 1;
    tag2->station = 1;
    tag2->area    = 1;
    tag2->address = 2;

    tag3->type    = 1;
    tag3->station = 1;
    tag3->area    = 1;
    tag3->address = 3;

    tag4->type    = 1;
    tag4->station = 1;
    tag4->area    = 1;
    tag4->address = 3;

    utarray_new(tags, &ut_ptr_icd);
    utarray_push_back(tags, &tag4);
    utarray_push_back(tags, &tag2);
    utarray_push_back(tags, &tag1);
    utarray_push_back(tags, &tag3);

    result = neu_tag_sort(tags, tag_sort_fn, tag_cmp);

    EXPECT_EQ(1, result->n_sort);
    EXPECT_EQ(result->sorts->info.size, 4);

    struct tag **p_tag =
        (struct tag **) utarray_eltptr(result->sorts[0].tags, 0);
    EXPECT_EQ((*p_tag)->address, 1);

    p_tag = (struct tag **) utarray_eltptr(result->sorts[0].tags, 1);
    EXPECT_EQ((*p_tag)->address, 2);

    p_tag = (struct tag **) utarray_eltptr(result->sorts[0].tags, 2);
    EXPECT_EQ((*p_tag)->address, 3);

    p_tag = (struct tag **) utarray_eltptr(result->sorts[0].tags, 3);
    EXPECT_EQ((*p_tag)->address, 3);

    neu_tag_sort_free(result);
    utarray_free(tags);

    free(tag1);
    free(tag2);
    free(tag3);
    free(tag4);
}

TEST(TagSortTest, SortDiff)
{
    UT_array *             tags   = NULL;
    neu_tag_sort_result_t *result = NULL;
    struct tag *           tag1 = (struct tag *) calloc(1, sizeof(struct tag));
    struct tag *           tag2 = (struct tag *) calloc(1, sizeof(struct tag));
    struct tag *           tag3 = (struct tag *) calloc(1, sizeof(struct tag));
    struct tag *           tag4 = (struct tag *) calloc(1, sizeof(struct tag));

    tag1->type    = 1;
    tag1->station = 1;
    tag1->area    = 1;
    tag1->address = 1;

    tag2->type    = 1;
    tag2->station = 1;
    tag2->area    = 1;
    tag2->address = 2;

    tag3->type    = 1;
    tag3->station = 1;
    tag3->area    = 1;
    tag3->address = 3;

    tag4->type    = 1;
    tag4->station = 2;
    tag4->area    = 1;
    tag4->address = 3;

    utarray_new(tags, &ut_ptr_icd);
    utarray_push_back(tags, &tag4);
    utarray_push_back(tags, &tag2);
    utarray_push_back(tags, &tag1);
    utarray_push_back(tags, &tag3);

    result = neu_tag_sort(tags, tag_sort_fn, tag_cmp);

    EXPECT_EQ(2, result->n_sort);
    EXPECT_EQ(result->sorts[0].info.size, 3);
    EXPECT_EQ(result->sorts[1].info.size, 1);

    struct tag **p_tag =
        (struct tag **) utarray_eltptr(result->sorts[0].tags, 0);
    EXPECT_EQ((*p_tag)->address, 1);

    p_tag = (struct tag **) utarray_eltptr(result->sorts[0].tags, 1);
    EXPECT_EQ((*p_tag)->address, 2);

    p_tag = (struct tag **) utarray_eltptr(result->sorts[0].tags, 2);
    EXPECT_EQ((*p_tag)->address, 3);
    EXPECT_EQ((*p_tag)->station, 1);

    p_tag = (struct tag **) utarray_eltptr(result->sorts[1].tags, 0);
    EXPECT_EQ((*p_tag)->address, 3);
    EXPECT_EQ((*p_tag)->station, 2);

    neu_tag_sort_free(result);
    utarray_free(tags);

    free(tag1);
    free(tag2);
    free(tag3);
    free(tag4);
}

TEST(TagSortTest, SortByFields)
{
    neu_datatag_t a = {}, b = {};
    a.name = a.address = a.description = a.unit = (char *) "a";
    b.name = b.address = b.description = b.unit = (char *) "b";
    a.type                                      = NEU_TYPE_INT16;
    b.type                                      = NEU_TYPE_INT32;
    a.attribute                                 = NEU_ATTRIBUTE_READ;
    b.attribute                                 = NEU_ATTRIBUTE_WRITE;
    a.precision                                 = 1;
    b.precision                                 = 2;
    a.decimal = a.bias = -1.5;
    b.decimal = b.bias = 2.5;

    for (const char *field : { "name", "address", "description", "unit", "type",
                               "attribute", "precision", "decimal", "bias" }) {
        SCOPED_TRACE(field);
        auto asc  = neu_tag_get_sort_cmp((std::string("+") + field).c_str());
        auto desc = neu_tag_get_sort_cmp((std::string("-") + field).c_str());
        ASSERT_NE(nullptr, asc);
        ASSERT_NE(nullptr, desc);
        EXPECT_LT(asc(&a, &b), 0);
        EXPECT_GT(desc(&a, &b), 0);
        EXPECT_EQ(asc(&a, &a), 0);
    }

    a.description = a.unit = nullptr;
    EXPECT_LT(neu_tag_get_sort_cmp("+description")(&a, &b), 0);
    EXPECT_LT(neu_tag_get_sort_cmp("+unit")(&a, &b), 0);
    EXPECT_EQ(nullptr, neu_tag_get_sort_cmp(nullptr));
    for (const char *invalid : { "", "+", "-", "name", " name", "+unknown" }) {
        EXPECT_EQ(nullptr, neu_tag_get_sort_cmp(invalid));
    }
}

TEST(TagSortTest, SortBeforePagination)
{
    neu_group_t *group = neu_group_new("group", 1000);
    ASSERT_NE(nullptr, group);
    for (const char *name : { "tag-c", "tag-a", "tag-b" }) {
        neu_datatag_t tag = {};
        tag.name          = (char *) name;
        tag.attribute     = NEU_ATTRIBUTE_READ;
        ASSERT_EQ(0, neu_group_add_tag(group, &tag));
    }

    for (const char *sort_by : { "+name", "-name" }) {
        auto        cmp   = neu_tag_get_sort_cmp(sort_by);
        const char *first = sort_by[0] == '+' ? "tag-a" : "tag-c";
        const char *last  = sort_by[0] == '+' ? "tag-c" : "tag-a";
        int         total = 0;
        UT_array *  tags  = neu_group_query_read_tag_paginate(
            group, "tag", nullptr, 1, 2, &total, cmp);
        EXPECT_EQ(3, total);
        ASSERT_EQ(2, utarray_len(tags));
        EXPECT_STREQ(first, ((neu_datatag_t *) utarray_front(tags))->name);
        EXPECT_STREQ("tag-b", ((neu_datatag_t *) utarray_back(tags))->name);
        utarray_free(tags);

        tags = neu_group_query_read_tag_paginate(group, "tag", nullptr, 2, 2,
                                                 &total, cmp);
        ASSERT_EQ(1, utarray_len(tags));
        EXPECT_STREQ(last, ((neu_datatag_t *) utarray_front(tags))->name);
        utarray_free(tags);

        tags = neu_group_query_read_tag_paginate(group, "tag", nullptr, 3, 2,
                                                 &total, cmp);
        EXPECT_EQ(0, utarray_len(tags));
        EXPECT_EQ(3, total);
        utarray_free(tags);
    }

    int       total = 0;
    UT_array *tags  = neu_group_query_read_tag_paginate(group, nullptr, nullptr,
                                                       1, 2, &total, nullptr);
    EXPECT_STREQ("tag-c", ((neu_datatag_t *) utarray_front(tags))->name);
    utarray_free(tags);
    neu_group_destroy(group);
}

TEST(TagSortTest, DecodePaginateSortBy)
{
    for (const char *sort_by : { "+name", "-address" }) {
        std::string body = std::string("{\"node\":\"node\",\"group\":\"group\","
                                       "\"query\":{\"sort_by\":\"") +
            sort_by + "\"}}";
        neu_json_read_paginate_req_t *req = nullptr;
        ASSERT_EQ(0, neu_json_decode_read_paginate_req(&body[0], &req));
        ASSERT_NE(nullptr, req);
        EXPECT_STREQ(sort_by, req->sort_by);
        neu_json_decode_read_paginate_req_free(req);
    }
    char body[] = "{\"node\":\"node\",\"group\":\"group\"}";
    neu_json_read_paginate_req_t *req = nullptr;
    ASSERT_EQ(0, neu_json_decode_read_paginate_req(body, &req));
    EXPECT_EQ(nullptr, req->sort_by);
    neu_json_decode_read_paginate_req_free(req);
}

int main(int argc, char **argv)
{
    zlog_init("./config/dev.conf");
    neuron = zlog_get_category("neuron");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}