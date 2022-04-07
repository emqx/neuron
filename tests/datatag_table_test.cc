#include <gtest/gtest.h>
#include <pthread.h>
#include <random>

#include "datatag_table.h"
#include "neu_tag.h"
#include "neu_vector.h"

#define MAX_NAME_LEN 128
#define NUM_DATATAGTABLE_MODIFY 100

static neu_attribute_e rand_attr()
{
    static std::default_random_engine         e;
    static std::uniform_int_distribution<int> d(0, 2);
    auto                                      i = d(e);
    switch (i) {
    case 0:
        return NEU_ATTRIBUTE_READ;
    case 1:
        return NEU_ATTRIBUTE_WRITE;
    default:
        return NEU_ATTRIBUTE_SUBSCRIBE;
    }
}

static neu_dtype_e rand_type()
{
    static std::default_random_engine         e;
    static std::uniform_int_distribution<int> d(NEU_DTYPE_BYTE, NEU_DTYPE_BIT);
    return (neu_dtype_e) d(e);
}

static neu_datatag_t *rand_tag(const char *addr_str, const char *fmt, ...)
{
    va_list args;
    char    name[MAX_NAME_LEN] = { 0 };

    va_start(args, fmt);
    vsnprintf(name, MAX_NAME_LEN, fmt, args);
    va_end(args);

    return neu_datatag_alloc(rand_attr(), rand_type(), (char *) addr_str, name);
}

TEST(DatatageTableTest, DatatagTableAdd)
{
    neu_datatag_table_t *datatag_table = neu_datatag_tbl_create();
    const char *         addr_str      = "1!neu.type_bit";
    neu_datatag_t *      datatag1      = rand_tag(addr_str, "tag1");
    neu_datatag_t *      datatag2      = rand_tag(addr_str, "tag2");
    neu_datatag_t *      datatag3      = rand_tag(addr_str, "tag3");

    datatag_id_t   datatag_id1, datatag_id2, datatag_id3;
    neu_datatag_t *datatag_id_return1 = NULL;
    neu_datatag_t *datatag_id_return2 = NULL;
    neu_datatag_t *datatag_id_return3 = NULL;

    datatag_id1 = neu_datatag_tbl_add(datatag_table, datatag1);
    datatag_id2 = neu_datatag_tbl_add(datatag_table, datatag2);
    datatag_id3 = neu_datatag_tbl_add(datatag_table, datatag3);

    datatag_id_return1 = neu_datatag_tbl_get(datatag_table, datatag_id1);
    datatag_id_return2 = neu_datatag_tbl_get(datatag_table, datatag_id2);
    datatag_id_return3 = neu_datatag_tbl_get(datatag_table, datatag_id3);

    EXPECT_EQ(datatag_id_return1->id, datatag1->id);
    EXPECT_EQ(datatag_id_return1->type, datatag1->type);
    EXPECT_EQ(datatag_id_return1->attribute, datatag1->attribute);
    EXPECT_EQ(datatag_id_return1->addr_str, datatag1->addr_str);

    EXPECT_EQ(datatag_id_return2->id, datatag2->id);
    EXPECT_EQ(datatag_id_return2->type, datatag2->type);
    EXPECT_EQ(datatag_id_return2->attribute, datatag2->attribute);
    EXPECT_EQ(datatag_id_return2->addr_str, datatag2->addr_str);

    EXPECT_EQ(datatag_id_return3->id, datatag3->id);
    EXPECT_EQ(datatag_id_return3->type, datatag3->type);
    EXPECT_EQ(datatag_id_return3->attribute, datatag3->attribute);
    EXPECT_EQ(datatag_id_return3->addr_str, datatag3->addr_str);

    EXPECT_EQ(0, neu_datatag_tbl_add(datatag_table, datatag1));
    EXPECT_EQ(0, neu_datatag_tbl_add(datatag_table, datatag2));
    EXPECT_EQ(0, neu_datatag_tbl_add(datatag_table, datatag3));
    EXPECT_EQ(3, neu_datatag_tbl_size(datatag_table));

    neu_datatag_tbl_destroy(datatag_table);
}

TEST(DatatageTableTest, DatatagTableRemove)
{
    neu_datatag_table_t *datatag_table = neu_datatag_tbl_create();
    neu_datatag_id_t     id;
    const char *         addr_str = "1!neu.type_bit";
    neu_datatag_t *      datatag1 = rand_tag(addr_str, "tag1");
    neu_datatag_t *      datatag2 = rand_tag(addr_str, "tag2");
    neu_datatag_t *      datatag3 = rand_tag(addr_str, "tag3");

    datatag_id_t   datatag_id1, datatag_id2, datatag_id3;
    neu_datatag_t *datatag_id_return1 = NULL;
    neu_datatag_t *datatag_id_return2 = NULL;
    neu_datatag_t *datatag_id_return3 = NULL;

    datatag_id1 = neu_datatag_tbl_add(datatag_table, datatag1);
    EXPECT_EQ(1, neu_datatag_tbl_size(datatag_table));
    EXPECT_EQ(0, neu_datatag_tbl_remove(datatag_table, datatag_id1));
    datatag_id_return1 = neu_datatag_tbl_get(datatag_table, datatag_id1);
    EXPECT_EQ(NULL, datatag_id_return1);
    EXPECT_EQ(0, neu_datatag_tbl_size(datatag_table));

    datatag1 = rand_tag(addr_str, "tag1");

    datatag_id1 = neu_datatag_tbl_add(datatag_table, datatag1);
    datatag_id2 = neu_datatag_tbl_add(datatag_table, datatag2);
    datatag_id3 = neu_datatag_tbl_add(datatag_table, datatag3);

    datatag_id_return1 = neu_datatag_tbl_get(datatag_table, datatag_id1);
    datatag_id_return2 = neu_datatag_tbl_get(datatag_table, datatag_id2);
    datatag_id_return3 = neu_datatag_tbl_get(datatag_table, datatag_id3);

    EXPECT_EQ(datatag_id_return1->id, datatag1->id);
    EXPECT_EQ(datatag_id_return1->type, datatag1->type);
    EXPECT_EQ(datatag_id_return1->attribute, datatag1->attribute);
    EXPECT_EQ(datatag_id_return1->addr_str, datatag1->addr_str);

    EXPECT_EQ(datatag_id_return2->id, datatag2->id);
    EXPECT_EQ(datatag_id_return2->type, datatag2->type);
    EXPECT_EQ(datatag_id_return2->attribute, datatag2->attribute);
    EXPECT_EQ(datatag_id_return2->addr_str, datatag2->addr_str);

    EXPECT_EQ(datatag_id_return3->id, datatag3->id);
    EXPECT_EQ(datatag_id_return3->type, datatag3->type);
    EXPECT_EQ(datatag_id_return3->attribute, datatag3->attribute);
    EXPECT_EQ(datatag_id_return3->addr_str, datatag3->addr_str);

    neu_datatag_tbl_destroy(datatag_table);
}

TEST(DatatageTableTest, DatatagTableUpdate)
{
    neu_datatag_table_t *datatag_table = neu_datatag_tbl_create();
    neu_datatag_id_t     id;
    const char *         addr_str = "1!neu.type_bit";
    neu_datatag_t *      datatag1 = rand_tag(addr_str, "foo");
    neu_datatag_t *      datatag2 = rand_tag(addr_str, "bar");
    neu_datatag_t *      datatag3 = rand_tag(addr_str, "bar");
    neu_datatag_t *      datatag4 = rand_tag(addr_str, "baz");

    datatag_id_t   datatag_id1, datatag_id2;
    neu_datatag_t *datatag_id_return = NULL;

    datatag_id1 = neu_datatag_tbl_add(datatag_table, datatag1);
    datatag_id2 = neu_datatag_tbl_add(datatag_table, datatag2);

    EXPECT_NE(0, neu_datatag_tbl_update(datatag_table, 0, datatag1));
    EXPECT_NE(0, neu_datatag_tbl_update(datatag_table, datatag_id1, datatag1));
    EXPECT_NE(0, neu_datatag_tbl_update(datatag_table, datatag_id1, datatag3));

    EXPECT_EQ(0, neu_datatag_tbl_update(datatag_table, datatag_id1, datatag4));

    datatag_id_return = neu_datatag_tbl_get(datatag_table, datatag_id1);

    EXPECT_EQ(datatag_id_return->attribute, datatag4->attribute);
    EXPECT_EQ(datatag_id_return->type, datatag4->type);
    EXPECT_EQ(datatag_id_return->id, datatag4->id);
    EXPECT_EQ(datatag_id_return->addr_str, datatag4->addr_str);
    EXPECT_EQ(2, neu_datatag_tbl_size(datatag_table));

    EXPECT_EQ(0, neu_datatag_tbl_remove(datatag_table, datatag_id2));
    EXPECT_EQ(0, neu_datatag_tbl_update(datatag_table, datatag_id1, datatag3));

    datatag_id_return = neu_datatag_tbl_get(datatag_table, datatag_id1);

    EXPECT_EQ(datatag_id_return->attribute, datatag3->attribute);
    EXPECT_EQ(datatag_id_return->type, datatag3->type);
    EXPECT_EQ(datatag_id_return->id, datatag3->id);
    EXPECT_EQ(datatag_id_return->addr_str, datatag3->addr_str);

    neu_datatag_tbl_destroy(datatag_table);
}

TEST(DatatageTableTest, DatatagTableToVec)
{
    int                  rv;
    neu_datatag_table_t *datatag_table = neu_datatag_tbl_create();
    const char *         addr_str      = "1!neu.type_bit";
    neu_datatag_t *      datatag1      = rand_tag(addr_str, "tag1");
    neu_datatag_t *      datatag2      = rand_tag(addr_str, "tag2");
    neu_datatag_t *      datatag3      = rand_tag(addr_str, "tag3");
    vector_t *           datatags_vec;

    datatag_id_t   datatag_id1, datatag_id2, datatag_id3;
    neu_datatag_t *datatag_id_return1 = NULL;
    neu_datatag_t *datatag_id_return2 = NULL;
    neu_datatag_t *datatag_id_return3 = NULL;

    datatag_id1 = neu_datatag_tbl_add(datatag_table, datatag1);
    datatag_id2 = neu_datatag_tbl_add(datatag_table, datatag2);
    datatag_id3 = neu_datatag_tbl_add(datatag_table, datatag3);

    rv = neu_datatag_tbl_to_vector(datatag_table, &datatags_vec);
    EXPECT_EQ(rv, 0);

    datatag_id_return1 = (neu_datatag_t *) vector_get(datatags_vec, 0);
    datatag_id_return2 = (neu_datatag_t *) vector_get(datatags_vec, 1);
    datatag_id_return3 = (neu_datatag_t *) vector_get(datatags_vec, 2);

    EXPECT_EQ(datatag_id_return1->id, datatag1->id);
    EXPECT_EQ(datatag_id_return1->type, datatag1->type);
    EXPECT_EQ(datatag_id_return1->attribute, datatag1->attribute);
    EXPECT_EQ(datatag_id_return1->addr_str, datatag1->addr_str);

    EXPECT_EQ(datatag_id_return2->id, datatag2->id);
    EXPECT_EQ(datatag_id_return2->type, datatag2->type);
    EXPECT_EQ(datatag_id_return2->attribute, datatag2->attribute);
    EXPECT_EQ(datatag_id_return2->addr_str, datatag2->addr_str);

    EXPECT_EQ(datatag_id_return3->id, datatag3->id);
    EXPECT_EQ(datatag_id_return3->type, datatag3->type);
    EXPECT_EQ(datatag_id_return3->attribute, datatag3->attribute);
    EXPECT_EQ(datatag_id_return3->addr_str, datatag3->addr_str);

    EXPECT_EQ(0, neu_datatag_tbl_add(datatag_table, datatag1));
    EXPECT_EQ(0, neu_datatag_tbl_add(datatag_table, datatag2));
    EXPECT_EQ(0, neu_datatag_tbl_add(datatag_table, datatag3));

    vector_free(datatags_vec);
    neu_datatag_tbl_destroy(datatag_table);
}

void *datatag_table_thread(void *arg)
{
    neu_datatag_table_t *datatag_table = (neu_datatag_table_t *) arg;
    datatag_id_t         id;
    const char *         addr_str = "1!neu.type_bit";
    pthread_t            tid      = pthread_self();

    datatag_id_t   datatag_id1, datatag_id2, datatag_id3;
    neu_datatag_t *datatag_id_return1 = NULL;
    neu_datatag_t *datatag_id_return2 = NULL;
    neu_datatag_t *datatag_id_return3 = NULL;

    for (int i = 0; i < NUM_DATATAGTABLE_MODIFY; ++i) {
        neu_datatag_t *datatag1 = rand_tag(addr_str, "tag-%u-1-%d", tid, i);
        neu_datatag_t *datatag2 = rand_tag(addr_str, "tag-%u-2-%d", tid, i);
        neu_datatag_t *datatag3 = rand_tag(addr_str, "tag-%u-3-%d", tid, i);

        datatag_id1 = neu_datatag_tbl_add(datatag_table, datatag1);
        datatag_id2 = neu_datatag_tbl_add(datatag_table, datatag2);
        datatag_id3 = neu_datatag_tbl_add(datatag_table, datatag3);

        EXPECT_EQ(0, neu_datatag_tbl_remove(datatag_table, datatag_id1));

        EXPECT_NE(0,
                  neu_datatag_tbl_update(datatag_table, datatag_id2, datatag2));
        EXPECT_NE(0,
                  neu_datatag_tbl_update(datatag_table, datatag_id3, datatag3));

        datatag2 = rand_tag(addr_str, "tag-%d-2-%d", tid, i);
        datatag3 = rand_tag(addr_str, "tag-%d-3-%d", tid, i);

        EXPECT_EQ(0,
                  neu_datatag_tbl_update(datatag_table, datatag_id2, datatag2));
        EXPECT_EQ(0,
                  neu_datatag_tbl_update(datatag_table, datatag_id3, datatag3));

        datatag_id_return1 = neu_datatag_tbl_get(datatag_table, datatag_id1);
        datatag_id_return2 = neu_datatag_tbl_get(datatag_table, datatag_id2);
        datatag_id_return3 = neu_datatag_tbl_get(datatag_table, datatag_id3);

        EXPECT_EQ(NULL, datatag_id_return1);

        EXPECT_EQ(datatag_id_return2->id, datatag2->id);
        EXPECT_EQ(datatag_id_return2->attribute, datatag2->attribute);
        EXPECT_EQ(datatag_id_return2->type, datatag2->type);
        EXPECT_EQ(datatag_id_return2->addr_str, datatag2->addr_str);

        EXPECT_EQ(datatag_id_return3->id, datatag3->id);
        EXPECT_EQ(datatag_id_return3->attribute, datatag3->attribute);
        EXPECT_EQ(datatag_id_return3->type, datatag3->type);
        EXPECT_EQ(datatag_id_return3->addr_str, datatag3->addr_str);
    }

    return 0;
}

TEST(DatatageTableTest, DatatagTablePthread)
{
    pthread_t            tids[2];
    int                  ret1, ret2;
    neu_datatag_table_t *datatag_table = neu_datatag_tbl_create();
    ret1 = pthread_create(&tids[0], NULL, datatag_table_thread,
                          (void *) datatag_table);
    ret2 = pthread_create(&tids[1], NULL, datatag_table_thread,
                          (void *) datatag_table);

    pthread_join(tids[0], NULL);
    pthread_join(tids[1], NULL);

    EXPECT_EQ(0, ret1);
    EXPECT_EQ(0, ret2);

    neu_datatag_tbl_destroy(datatag_table);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
