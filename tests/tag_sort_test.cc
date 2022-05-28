#include <stdio.h>
#include <stdlib.h>

#include <gtest/gtest.h>

#include "tag_sort.h"

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

int main(int argc, char **argv)
{
    zlog_init("./config/dev.conf");
    neuron = zlog_get_category("neuron");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}