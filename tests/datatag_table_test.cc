#include <gtest/gtest.h>
#include "neu_datatag_table.h"
#include "neu_types.h"
#include "pthread.h"
#include <bits/pthreadtypes.h>

#define NUM_DatatagTable_Modify 100

TEST(DatatageTableTest,DatatagTableAdd)
{
    neu_datatag_table_t *datatag_table = neu_datatag_tbl_create();
    neu_address_t address_t;
    neu_datatag_t datatag_t_1 = {123,NEU_ATTRIBUTETYPE_READ,NEU_DATATYPE_BYTE,address_t};
    neu_datatag_t datatag_t_2 = {456,NEU_ATTRIBUTETYPE_WRITE,NEU_DATATYPE_WORD,address_t};
    neu_datatag_t datatag_t_3 = {789,NEU_ATTRIBUTETYPE_SUBSCRIBE,NEU_DATATYPE_UWORD,address_t};
  
    datatag_id_t  datatag_add_1,datatag_add_2,datatag_add_3;
    neu_datatag_t *datatag_get_1 = NULL;
    neu_datatag_t *datatag_get_2 = NULL;
    neu_datatag_t *datatag_get_3 = NULL;
       
    datatag_add_1 = neu_datatag_tbl_add(datatag_table, &datatag_t_1);
    datatag_add_2 = neu_datatag_tbl_add(datatag_table, &datatag_t_2);
    datatag_add_3 = neu_datatag_tbl_add(datatag_table, &datatag_t_3);

    datatag_get_1 = neu_datatag_tbl_get(datatag_table, datatag_add_1);
    datatag_get_2 = neu_datatag_tbl_get(datatag_table, datatag_add_2);
    datatag_get_3 = neu_datatag_tbl_get(datatag_table, datatag_add_3);

    EXPECT_EQ(datatag_get_1->id,datatag_t_1.id);
    EXPECT_EQ(datatag_get_1->dataType,datatag_t_1.dataType);
    EXPECT_EQ(datatag_get_1->type,datatag_t_1.type);

    EXPECT_EQ(datatag_get_2->id,datatag_t_2.id);
    EXPECT_EQ(datatag_get_2->dataType,datatag_t_2.dataType);
    EXPECT_EQ(datatag_get_2->type,datatag_t_2.type);

    EXPECT_EQ(datatag_get_3->id,datatag_t_3.id);
    EXPECT_EQ(datatag_get_3->dataType,datatag_t_3.dataType);
    EXPECT_EQ(datatag_get_3->type,datatag_t_3.type);

    neu_datatag_tbl_destroy(datatag_table);
}

TEST(DatatageTableTest,DatatagTableRemove)
{
    neu_datatag_table_t *datatag_table = neu_datatag_tbl_create();
    neu_address_t address_t;
    neu_datatag_t datatag_t_1 = {123,NEU_ATTRIBUTETYPE_READ,NEU_DATATYPE_BYTE,address_t};
    neu_datatag_t datatag_t_2 = {456,NEU_ATTRIBUTETYPE_WRITE,NEU_DATATYPE_WORD,address_t};
    neu_datatag_t datatag_t_3 = {789,NEU_ATTRIBUTETYPE_SUBSCRIBE,NEU_DATATYPE_UWORD,address_t};

    datatag_id_t  datatag_add_1,datatag_add_2,datatag_add_3;
    neu_datatag_t *datatag_get_1 = NULL;
    neu_datatag_t *datatag_get_2 = NULL;
    neu_datatag_t *datatag_get_3 = NULL;  

    datatag_add_1 = neu_datatag_tbl_add(datatag_table, &datatag_t_1);
    datatag_add_2 = neu_datatag_tbl_add(datatag_table, &datatag_t_2);
    datatag_add_3 = neu_datatag_tbl_add(datatag_table, &datatag_t_3);

    EXPECT_EQ(0,neu_datatag_tbl_remove(datatag_table, datatag_add_1));

    datatag_get_1 = neu_datatag_tbl_get(datatag_table, datatag_add_1);
    datatag_get_2 = neu_datatag_tbl_get(datatag_table, datatag_add_2);
    datatag_get_3 = neu_datatag_tbl_get(datatag_table, datatag_add_3);

    EXPECT_EQ(NULL,datatag_get_1);
   
    EXPECT_EQ(datatag_get_2->id,datatag_t_2.id);
    EXPECT_EQ(datatag_get_2->dataType,datatag_t_2.dataType);
    EXPECT_EQ(datatag_get_2->type,datatag_t_2.type);

    EXPECT_EQ(datatag_get_3->id,datatag_t_3.id);
    EXPECT_EQ(datatag_get_3->dataType,datatag_t_3.dataType);
    EXPECT_EQ(datatag_get_3->type,datatag_t_3.type);

    neu_datatag_tbl_destroy(datatag_table);
}

TEST(DatatageTableTest,DatatagTableUpdate)
{
    neu_datatag_table_t *datatag_table = neu_datatag_tbl_create();
    neu_address_t address_t;
    neu_datatag_t datatag_t = {123,NEU_ATTRIBUTETYPE_READ,NEU_DATATYPE_BYTE,address_t};

    datatag_id_t  datatag_add;
    neu_datatag_t *datatag_get = NULL;

    datatag_add = neu_datatag_tbl_add(datatag_table, &datatag_t);
    datatag_t.id = 456;
    EXPECT_EQ(0,neu_datatag_tbl_update(datatag_table,datatag_add,&datatag_t));
    datatag_get = neu_datatag_tbl_get(datatag_table, datatag_add);

    EXPECT_EQ(456,datatag_get->id);

    neu_datatag_tbl_destroy(datatag_table);
}

void * datatag_table_thread(void * arg)
{
    neu_datatag_table_t *datatag_table = (neu_datatag_table_t *)arg;
    neu_address_t address_t;
    neu_datatag_t datatag_t_1 = {123,NEU_ATTRIBUTETYPE_READ,NEU_DATATYPE_BYTE,address_t};
    neu_datatag_t datatag_t_2 = {456,NEU_ATTRIBUTETYPE_WRITE,NEU_DATATYPE_WORD,address_t};
    neu_datatag_t datatag_t_3 = {789,NEU_ATTRIBUTETYPE_SUBSCRIBE,NEU_DATATYPE_UWORD,address_t};
  
    datatag_id_t  datatag_add_1,datatag_add_2,datatag_add_3,datatag_add_4;
    neu_datatag_t *datatag_get_1 = NULL;
    neu_datatag_t *datatag_get_2 = NULL;
    neu_datatag_t *datatag_get_3 = NULL;

    for(int i = 0; i < NUM_DatatagTable_Modify; ++i )
    {
      datatag_add_1 = neu_datatag_tbl_add(datatag_table, &datatag_t_1);
      datatag_add_2 = neu_datatag_tbl_add(datatag_table, &datatag_t_2);
      datatag_add_3 = neu_datatag_tbl_add(datatag_table, &datatag_t_3);

      EXPECT_EQ(0,neu_datatag_tbl_remove(datatag_table, datatag_add_1));

      datatag_get_1 = neu_datatag_tbl_get(datatag_table, datatag_add_1);
      datatag_get_2 = neu_datatag_tbl_get(datatag_table, datatag_add_2);
      datatag_get_3 = neu_datatag_tbl_get(datatag_table, datatag_add_3);
      
      datatag_t_2.id = 123;
      datatag_t_3.type = NEU_ATTRIBUTETYPE_WRITE;
      EXPECT_EQ(0,neu_datatag_tbl_update(datatag_table,datatag_add_2,&datatag_t_2));
      EXPECT_EQ(0,neu_datatag_tbl_update(datatag_table,datatag_add_3,&datatag_t_3));

      datatag_get_1 = neu_datatag_tbl_get(datatag_table, datatag_add_1);
      datatag_get_2 = neu_datatag_tbl_get(datatag_table, datatag_add_2);
      datatag_get_3 = neu_datatag_tbl_get(datatag_table, datatag_add_3);

      EXPECT_EQ(NULL,datatag_get_1);

      EXPECT_EQ(datatag_get_2->id,123);
      EXPECT_EQ(datatag_get_2->dataType,datatag_t_2.dataType);
      EXPECT_EQ(datatag_get_2->type,datatag_t_2.type);

      EXPECT_EQ(datatag_get_3->id,datatag_t_3.id);
      EXPECT_EQ(datatag_get_3->dataType,datatag_t_3.dataType);
      EXPECT_EQ(datatag_get_3->type,NEU_ATTRIBUTETYPE_WRITE);

    }
    return 0;
}


TEST(DatatageTableTest,DatatagTablePthread)
{
    pthread_t tids[2];
    int ret1,ret2;
    neu_datatag_table_t *datatag_table = neu_datatag_tbl_create();

    ret1 = pthread_create(&tids[0],NULL,datatag_table_thread,(void*)datatag_table);
    ret2 = pthread_create(&tids[1],NULL,datatag_table_thread,(void*)datatag_table);
    
    pthread_join(tids[0],NULL);
    pthread_join(tids[1],NULL);

    EXPECT_EQ(0,ret1);
    EXPECT_EQ(0,ret2);

    neu_datatag_tbl_destroy(datatag_table);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

