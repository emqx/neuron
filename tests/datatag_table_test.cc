#include "neu_datatag_table.h"
#include "neu_types.h"
#include "pthread.h"
#include <bits/pthreadtypes.h>
#include <gtest/gtest.h>

#define NUM_DATATAGTABLE_MODIFY 100

TEST(DatatageTableTest, DatatagTableAdd)
{
    neu_datatag_table_t *datatag_table_create = neu_datatag_tbl_create();
    neu_datatag_id_t     id;
    neu_address_t        datatag_address;
    char *               str_addr;
    neu_datatag_t datatag1 = { id, NEU_ATTRIBUTETYPE_READ, NEU_DATATYPE_BYTE,
                               datatag_address, str_addr };
    neu_datatag_t datatag2 = { id, NEU_ATTRIBUTETYPE_WRITE, NEU_DATATYPE_WORD,
                               datatag_address, str_addr };
    neu_datatag_t datatag3 = { id, NEU_ATTRIBUTETYPE_SUBSCRIBE,
                               NEU_DATATYPE_UWORD, datatag_address, str_addr };

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
    EXPECT_EQ(datatag_id_return1->dataType, datatag1.dataType);
    EXPECT_EQ(datatag_id_return1->type, datatag1.type);
    EXPECT_EQ(datatag_id_return1->str_addr, datatag1.str_addr);

    EXPECT_EQ(datatag_id_return2->id, datatag2.id);
    EXPECT_EQ(datatag_id_return2->dataType, datatag2.dataType);
    EXPECT_EQ(datatag_id_return2->type, datatag2.type);
    EXPECT_EQ(datatag_id_return2->str_addr, datatag2.str_addr);

    EXPECT_EQ(datatag_id_return3->id, datatag3.id);
    EXPECT_EQ(datatag_id_return3->dataType, datatag3.dataType);
    EXPECT_EQ(datatag_id_return3->type, datatag3.type);
    EXPECT_EQ(datatag_id_return3->str_addr, datatag3.str_addr);

    neu_datatag_tbl_destroy(datatag_table_create);
}

TEST(DatatageTableTest, DatatagTableRemove)
{
    neu_datatag_table_t *datatag_table_create = neu_datatag_tbl_create();
    neu_address_t        datatag_address;
    neu_datatag_id_t     id;
    char *               str_addr;
    neu_datatag_t datatag1 = { id, NEU_ATTRIBUTETYPE_READ, NEU_DATATYPE_BYTE,
                               datatag_address, str_addr };
    neu_datatag_t datatag2 = { id, NEU_ATTRIBUTETYPE_WRITE, NEU_DATATYPE_WORD,
                               datatag_address, str_addr };
    neu_datatag_t datatag3 = { id, NEU_ATTRIBUTETYPE_SUBSCRIBE,
                               NEU_DATATYPE_UWORD, datatag_address, str_addr };

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
    EXPECT_EQ(datatag_id_return2->dataType, datatag2.dataType);
    EXPECT_EQ(datatag_id_return2->type, datatag2.type);
    EXPECT_EQ(datatag_id_return2->str_addr, datatag2.str_addr);

    EXPECT_EQ(datatag_id_return3->id, datatag3.id);
    EXPECT_EQ(datatag_id_return3->dataType, datatag3.dataType);
    EXPECT_EQ(datatag_id_return3->type, datatag3.type);
    EXPECT_EQ(datatag_id_return3->str_addr, datatag3.str_addr);

    neu_datatag_tbl_destroy(datatag_table_create);
}

TEST(DatatageTableTest, DatatagTableUpdate)
{
    neu_datatag_table_t *datatag_table_create = neu_datatag_tbl_create();
    neu_address_t        datatag_address;
    neu_datatag_id_t     id;
    char *               str_addr;
    neu_datatag_t datatag = { id, NEU_ATTRIBUTETYPE_READ, NEU_DATATYPE_BYTE,
                              datatag_address, str_addr };

    datatag_id_t   datatag_id;
    neu_datatag_t *datatag_id_return = NULL;

    datatag_id = neu_datatag_tbl_add(datatag_table_create, &datatag);

    datatag.type     = NEU_ATTRIBUTETYPE_WRITE;
    datatag.dataType = NEU_DATATYPE_BOOLEAN;
    EXPECT_EQ(
        0, neu_datatag_tbl_update(datatag_table_create, datatag_id, &datatag));

    datatag_id_return = neu_datatag_tbl_get(datatag_table_create, datatag_id);

    EXPECT_EQ(datatag_id_return->type, NEU_ATTRIBUTETYPE_WRITE);
    EXPECT_EQ(datatag_id_return->dataType, NEU_DATATYPE_BOOLEAN);
    EXPECT_EQ(datatag_id_return->id, datatag.id);
    EXPECT_EQ(datatag_id_return->str_addr, datatag.str_addr);

    neu_datatag_tbl_destroy(datatag_table_create);
}

void *datatag_table_thread(void *arg)
{
    neu_datatag_table_t *datatag_table_create = (neu_datatag_table_t *) arg;
    neu_address_t        address;
    datatag_id_t         id;
    char *               str_addr;
    neu_datatag_t datatag1 = { id, NEU_ATTRIBUTETYPE_READ, NEU_DATATYPE_BYTE,
                               address, str_addr };
    neu_datatag_t datatag2 = { id, NEU_ATTRIBUTETYPE_WRITE, NEU_DATATYPE_WORD,
                               address, str_addr };
    neu_datatag_t datatag3 = { id, NEU_ATTRIBUTETYPE_SUBSCRIBE,
                               NEU_DATATYPE_UWORD, address, str_addr };

    datatag_id_t   datatag_id1, datatag_id2, datatag_id3;
    neu_datatag_t *datatag_id_return1 = NULL;
    neu_datatag_t *datatag_id_return2 = NULL;
    neu_datatag_t *datatag_id_return3 = NULL;

    for (int i = 0; i < NUM_DATATAGTABLE_MODIFY; ++i) {
        datatag_id1 = neu_datatag_tbl_add(datatag_table_create, &datatag1);
        datatag_id2 = neu_datatag_tbl_add(datatag_table_create, &datatag2);
        datatag_id3 = neu_datatag_tbl_add(datatag_table_create, &datatag3);

        EXPECT_EQ(0, neu_datatag_tbl_remove(datatag_table_create, datatag_id1));

        datatag2.type     = NEU_ATTRIBUTETYPE_WRITE;
        datatag3.dataType = NEU_DATATYPE_BOOLEAN;
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
        EXPECT_EQ(datatag_id_return2->type, NEU_ATTRIBUTETYPE_WRITE);
        EXPECT_EQ(datatag_id_return2->dataType, datatag2.dataType);
        EXPECT_EQ(datatag_id_return2->str_addr, datatag2.str_addr);

        EXPECT_EQ(datatag_id_return3->id, datatag3.id);
        EXPECT_EQ(datatag_id_return3->dataType, NEU_DATATYPE_BOOLEAN);
        EXPECT_EQ(datatag_id_return3->type, datatag3.type);
        EXPECT_EQ(datatag_id_return3->str_addr, datatag3.str_addr);
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
