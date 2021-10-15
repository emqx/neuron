#include <gtest/gtest.h>
#include <pthread.h>

#include "neu_datatag_table.h"
#include "neu_tag.h"

#define NUM_DATATAGTABLE_MODIFY 100

TEST(DatatageTableTest, DatatagTableAdd)
{
    neu_datatag_table_t *datatag_table_create = neu_datatag_tbl_create();
    char *               addr_str;
    neu_datatag_t        datatag1 = { 0, NEU_ATTRIBUTETYPE_READ, NEU_DTYPE_BIT,
                               addr_str };
    neu_datatag_t datatag2 = { 0, NEU_ATTRIBUTETYPE_WRITE, NEU_DTYPE_INT16,
                               addr_str };
    neu_datatag_t datatag3 = { 0, NEU_ATTRIBUTETYPE_SUBSCRIBE, NEU_DTYPE_UINT16,
                               addr_str };

    datatag_id_t   datatag_id1, datatag_id2, datatag_id3;
    neu_datatag_t *datatag_id_return1 = NULL;
    neu_datatag_t *datatag_id_return2 = NULL;
    neu_datatag_t *datatag_id_return3 = NULL;

    datatag_id1 = neu_datatag_tbl_add(datatag_table_create, &datatag1);
    datatag_id2 = neu_datatag_tbl_add(datatag_table_create, &datatag2);
    datatag_id3 = neu_datatag_tbl_add(datatag_table_create, &datatag3);

    datatag_id_return1 = neu_datatag_tbl_get(datatag_table_create, datatag_id1);
    datatag_id_return2 = neu_datatag_tbl_get(datatag_table_create, datatag_id2);
    datatag_id_return3 = neu_datatag_tbl_get(datatag_table_create, datatag_id3);

    EXPECT_EQ(datatag_id_return1->id, datatag1.id);
    EXPECT_EQ(datatag_id_return1->type, datatag1.type);
    EXPECT_EQ(datatag_id_return1->attribute, datatag1.attribute);
    EXPECT_EQ(datatag_id_return1->addr_str, datatag1.addr_str);

    EXPECT_EQ(datatag_id_return2->id, datatag2.id);
    EXPECT_EQ(datatag_id_return2->type, datatag2.type);
    EXPECT_EQ(datatag_id_return2->attribute, datatag2.attribute);
    EXPECT_EQ(datatag_id_return2->addr_str, datatag2.addr_str);

    EXPECT_EQ(datatag_id_return3->id, datatag3.id);
    EXPECT_EQ(datatag_id_return3->type, datatag3.type);
    EXPECT_EQ(datatag_id_return3->attribute, datatag3.attribute);
    EXPECT_EQ(datatag_id_return3->addr_str, datatag3.addr_str);

    neu_datatag_tbl_destroy(datatag_table_create);
}

TEST(DatatageTableTest, DatatagTableRemove)
{
    neu_datatag_table_t *datatag_table_create = neu_datatag_tbl_create();
    neu_datatag_id_t     id;
    char *               addr_str;
    neu_datatag_t        datatag1 = { id, NEU_ATTRIBUTETYPE_READ, NEU_DTYPE_BIT,
                               addr_str };
    neu_datatag_t datatag2 = { id, NEU_ATTRIBUTETYPE_WRITE, NEU_DTYPE_INT16,
                               addr_str };
    neu_datatag_t datatag3 = { id, NEU_ATTRIBUTETYPE_SUBSCRIBE,
                               NEU_DTYPE_UINT16, addr_str };

    datatag_id_t   datatag_id1, datatag_id2, datatag_id3;
    neu_datatag_t *datatag_id_return1 = NULL;
    neu_datatag_t *datatag_id_return2 = NULL;
    neu_datatag_t *datatag_id_return3 = NULL;

    datatag_id1 = neu_datatag_tbl_add(datatag_table_create, &datatag1);
    datatag_id2 = neu_datatag_tbl_add(datatag_table_create, &datatag2);
    datatag_id3 = neu_datatag_tbl_add(datatag_table_create, &datatag3);

    EXPECT_EQ(0, neu_datatag_tbl_remove(datatag_table_create, datatag_id1));

    datatag_id_return1 = neu_datatag_tbl_get(datatag_table_create, datatag_id1);
    datatag_id_return2 = neu_datatag_tbl_get(datatag_table_create, datatag_id2);
    datatag_id_return3 = neu_datatag_tbl_get(datatag_table_create, datatag_id3);

    EXPECT_EQ(NULL, datatag_id_return1);

    EXPECT_EQ(datatag_id_return2->id, datatag2.id);
    EXPECT_EQ(datatag_id_return2->type, datatag2.type);
    EXPECT_EQ(datatag_id_return2->attribute, datatag2.attribute);
    EXPECT_EQ(datatag_id_return2->addr_str, datatag2.addr_str);

    EXPECT_EQ(datatag_id_return3->id, datatag3.id);
    EXPECT_EQ(datatag_id_return3->type, datatag3.type);
    EXPECT_EQ(datatag_id_return3->attribute, datatag3.attribute);
    EXPECT_EQ(datatag_id_return3->addr_str, datatag3.addr_str);

    neu_datatag_tbl_destroy(datatag_table_create);
}

TEST(DatatageTableTest, DatatagTableUpdate)
{
    neu_datatag_table_t *datatag_table_create = neu_datatag_tbl_create();
    neu_datatag_id_t     id;
    char *               addr_str;
    neu_datatag_t        datatag = { id, NEU_ATTRIBUTETYPE_READ, NEU_DTYPE_BIT,
                              addr_str };

    datatag_id_t   datatag_id;
    neu_datatag_t *datatag_id_return = NULL;

    datatag_id = neu_datatag_tbl_add(datatag_table_create, &datatag);

    datatag.attribute = NEU_ATTRIBUTETYPE_WRITE;
    datatag.type      = NEU_DTYPE_BOOL;
    EXPECT_EQ(
        0, neu_datatag_tbl_update(datatag_table_create, datatag_id, &datatag));

    datatag_id_return = neu_datatag_tbl_get(datatag_table_create, datatag_id);

    EXPECT_EQ(datatag_id_return->attribute, NEU_ATTRIBUTETYPE_WRITE);
    EXPECT_EQ(datatag_id_return->type, NEU_DTYPE_BOOL);
    EXPECT_EQ(datatag_id_return->id, datatag.id);
    EXPECT_EQ(datatag_id_return->addr_str, datatag.addr_str);

    neu_datatag_tbl_destroy(datatag_table_create);
}

void *datatag_table_thread(void *arg)
{
    neu_datatag_table_t *datatag_table_create = (neu_datatag_table_t *) arg;
    datatag_id_t         id;
    char *               addr_str;
    neu_datatag_t        datatag1 = { id, NEU_ATTRIBUTETYPE_READ, NEU_DTYPE_BIT,
                               addr_str };
    neu_datatag_t datatag2 = { id, NEU_ATTRIBUTETYPE_WRITE, NEU_DTYPE_INT16,
                               addr_str };
    neu_datatag_t datatag3 = { id, NEU_ATTRIBUTETYPE_SUBSCRIBE,
                               NEU_DTYPE_UINT16, addr_str };

    datatag_id_t   datatag_id1, datatag_id2, datatag_id3;
    neu_datatag_t *datatag_id_return1 = NULL;
    neu_datatag_t *datatag_id_return2 = NULL;
    neu_datatag_t *datatag_id_return3 = NULL;

    for (int i = 0; i < NUM_DATATAGTABLE_MODIFY; ++i) {
        datatag_id1 = neu_datatag_tbl_add(datatag_table_create, &datatag1);
        datatag_id2 = neu_datatag_tbl_add(datatag_table_create, &datatag2);
        datatag_id3 = neu_datatag_tbl_add(datatag_table_create, &datatag3);

        EXPECT_EQ(0, neu_datatag_tbl_remove(datatag_table_create, datatag_id1));

        datatag2.attribute = NEU_ATTRIBUTETYPE_WRITE;
        datatag3.type      = NEU_DTYPE_BOOL;
        EXPECT_EQ(0,
                  neu_datatag_tbl_update(datatag_table_create, datatag_id2,
                                         &datatag2));
        EXPECT_EQ(0,
                  neu_datatag_tbl_update(datatag_table_create, datatag_id3,
                                         &datatag3));

        datatag_id_return1 =
            neu_datatag_tbl_get(datatag_table_create, datatag_id1);
        datatag_id_return2 =
            neu_datatag_tbl_get(datatag_table_create, datatag_id2);
        datatag_id_return3 =
            neu_datatag_tbl_get(datatag_table_create, datatag_id3);

        EXPECT_EQ(NULL, datatag_id_return1);

        EXPECT_EQ(datatag_id_return2->id, datatag2.id);
        EXPECT_EQ(datatag_id_return2->attribute, NEU_ATTRIBUTETYPE_WRITE);
        EXPECT_EQ(datatag_id_return2->type, datatag2.type);
        EXPECT_EQ(datatag_id_return2->addr_str, datatag2.addr_str);

        EXPECT_EQ(datatag_id_return3->id, datatag3.id);
        EXPECT_EQ(datatag_id_return3->type, NEU_DTYPE_BOOL);
        EXPECT_EQ(datatag_id_return3->attribute, datatag3.attribute);
        EXPECT_EQ(datatag_id_return3->addr_str, datatag3.addr_str);
    }
    return 0;
}

TEST(DatatageTableTest, DatatagTablePthread)
{
    pthread_t            tids[2];
    int                  ret1, ret2;
    neu_datatag_table_t *datatag_table_create = neu_datatag_tbl_create();
    ret1 = pthread_create(&tids[0], NULL, datatag_table_thread,
                          (void *) datatag_table_create);
    ret2 = pthread_create(&tids[1], NULL, datatag_table_thread,
                          (void *) datatag_table_create);

    pthread_join(tids[0], NULL);
    pthread_join(tids[1], NULL);

    EXPECT_EQ(0, ret1);
    EXPECT_EQ(0, ret2);

    neu_datatag_tbl_destroy(datatag_table_create);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
