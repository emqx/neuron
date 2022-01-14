#include <gtest/gtest.h>
#include <stdlib.h>

#include "core/plugin_manager.h"

#define NUM_DATATAGTABLE_MODIFY 100

TEST(PluginManagerTest, RegPluginManagerTest)
{
    plugin_manager_t *plugin_mng      = plugin_manager_create();
    vector_t *        plugin_info_vec = { 0 };

    int               index    = 0;
    plugin_reg_info_t reg_info = { 0 };

    plugin_reg_param_t param1 = { PLUGIN_KIND_SYSTEM, ADAPTER_TYPE_WEBSERVER,
                                  (char *) "plugin1", (char *) "lib1" };
    plugin_reg_param_t param2 = { PLUGIN_KIND_SYSTEM, ADAPTER_TYPE_WEBSERVER,
                                  (char *) "plugin2", (char *) "lib2" };
    plugin_reg_param_t param3 = { PLUGIN_KIND_SYSTEM, ADAPTER_TYPE_WEBSERVER,
                                  (char *) "plugin3", (char *) "lib3" };
    plugin_id_t        p_plugin_id = { 0 };

    plugin_id_t plugin_id1 = { 0 };
    plugin_id_t plugin_id2 = { 0 };
    plugin_id_t plugin_id3 = { 0 };

    EXPECT_EQ(0, plugin_manager_reg_plugin(plugin_mng, &param1, &p_plugin_id));
    EXPECT_EQ(0, plugin_manager_reg_plugin(plugin_mng, &param2, &p_plugin_id));
    EXPECT_EQ(0, plugin_manager_reg_plugin(plugin_mng, &param3, &p_plugin_id));

    /**plugin_manager_get_all_plugins**/
    plugin_info_vec = plugin_manager_get_all_plugins(plugin_mng);
    VECTOR_FOR_EACH(plugin_info_vec, iter)
    {
        plugin_reg_info_t *info = (plugin_reg_info_t *) iterator_get(&iter);

        switch (index) {
        case 2:
            plugin_id1 = info->plugin_id;

            EXPECT_EQ(1, info->plugin_kind);
            EXPECT_STREQ("plugin1", info->plugin_name);
            EXPECT_STREQ("lib1", info->plugin_lib_name);
            break;
        case 3:
            plugin_id2 = info->plugin_id;

            EXPECT_EQ(1, info->plugin_kind);
            EXPECT_STREQ("plugin2", info->plugin_name);
            EXPECT_STREQ("lib2", info->plugin_lib_name);
            break;
        case 4:
            plugin_id3 = info->plugin_id;

            EXPECT_EQ(1, info->plugin_kind);
            EXPECT_STREQ("plugin3", info->plugin_name);
            EXPECT_STREQ("lib3", info->plugin_lib_name);
            break;
        default:
            break;
        }
        index += 1;
    }

    /**plugin_manager_get_reg_info by plugin_id**/
    EXPECT_EQ(0,
              plugin_manager_get_reg_info(plugin_mng, plugin_id1, &reg_info));
    EXPECT_EQ(0,
              plugin_manager_get_reg_info(plugin_mng, plugin_id2, &reg_info));
    EXPECT_EQ(0,
              plugin_manager_get_reg_info(plugin_mng, plugin_id3, &reg_info));

    /**plugin_manager_get_reg_info_by_name**/
    EXPECT_EQ(0,
              plugin_manager_get_reg_info_by_name(
                  plugin_mng, (char *) "plugin1", &reg_info));
    EXPECT_EQ(0,
              plugin_manager_get_reg_info_by_name(
                  plugin_mng, (char *) "plugin2", &reg_info));
    EXPECT_EQ(0,
              plugin_manager_get_reg_info_by_name(
                  plugin_mng, (char *) "plugin3", &reg_info));

    vector_free(plugin_info_vec);
    plugin_manager_unreg_all_plugins(plugin_mng);
    plugin_manager_destroy(plugin_mng);
}

TEST(PluginManagerTest, UnregPluginManagerTest)
{
    plugin_manager_t *plugin_mng      = plugin_manager_create();
    vector_t *        plugin_info_vec = { 0 };

    int index = 0;

    plugin_id_t plugin_id1 = { 0 };
    plugin_id_t plugin_id2 = { 0 };
    plugin_id_t plugin_id3 = { 0 };

    plugin_reg_info_t reg_info = { 0 };

    plugin_reg_param_t param1 = { PLUGIN_KIND_SYSTEM, ADAPTER_TYPE_WEBSERVER,
                                  (char *) "plugin1", (char *) "lib1" };
    plugin_reg_param_t param2 = { PLUGIN_KIND_SYSTEM, ADAPTER_TYPE_WEBSERVER,
                                  (char *) "plugin2", (char *) "lib2" };
    plugin_reg_param_t param3 = { PLUGIN_KIND_SYSTEM, ADAPTER_TYPE_WEBSERVER,
                                  (char *) "plugin3", (char *) "lib3" };
    plugin_id_t        p_plugin_id = { 0 };

    EXPECT_EQ(0, plugin_manager_reg_plugin(plugin_mng, &param1, &p_plugin_id));
    EXPECT_EQ(0, plugin_manager_reg_plugin(plugin_mng, &param2, &p_plugin_id));
    EXPECT_EQ(0, plugin_manager_reg_plugin(plugin_mng, &param3, &p_plugin_id));

    plugin_info_vec = plugin_manager_get_all_plugins(plugin_mng);
    VECTOR_FOR_EACH(plugin_info_vec, iter)
    {
        plugin_reg_info_t *info = (plugin_reg_info_t *) iterator_get(&iter);

        switch (index) {
        case 2:
            plugin_id1 = info->plugin_id;
            break;
        case 3:
            plugin_id2 = info->plugin_id;
            break;
        case 4:
            plugin_id3 = info->plugin_id;
            break;
        default:
            break;
        }
        index += 1;
    }

    plugin_manager_unreg_plugin(plugin_mng, plugin_id1);
    EXPECT_EQ(-1,
              plugin_manager_get_reg_info(plugin_mng, plugin_id1, &reg_info));

    /***plugin_manager_unreg_all_plugins***/
    plugin_manager_unreg_all_plugins(plugin_mng);
    EXPECT_EQ(-1,
              plugin_manager_get_reg_info(plugin_mng, plugin_id1, &reg_info));
    EXPECT_EQ(-1,
              plugin_manager_get_reg_info(plugin_mng, plugin_id2, &reg_info));
    EXPECT_EQ(-1,
              plugin_manager_get_reg_info(plugin_mng, plugin_id3, &reg_info));

    vector_free(plugin_info_vec);
    plugin_manager_unreg_all_plugins(plugin_mng);
    plugin_manager_destroy(plugin_mng);
}

TEST(PluginManagerTest, UpdatePluginManagerTest)
{
    plugin_manager_t *plugin_mng      = plugin_manager_create();
    vector_t *        plugin_info_vec = { 0 };

    int               index    = 0;
    plugin_reg_info_t reg_info = { 0 };

    plugin_reg_param_t param1 = { PLUGIN_KIND_SYSTEM, ADAPTER_TYPE_WEBSERVER,
                                  (char *) "plugin1", (char *) "lib1" };
    plugin_reg_param_t param2 = { PLUGIN_KIND_SYSTEM, ADAPTER_TYPE_WEBSERVER,
                                  (char *) "plugin2", (char *) "lib2" };
    plugin_reg_param_t param3 = { PLUGIN_KIND_SYSTEM, ADAPTER_TYPE_WEBSERVER,
                                  (char *) "plugin3", (char *) "lib3" };
    plugin_reg_param_t param_update = { PLUGIN_KIND_SYSTEM,
                                        ADAPTER_TYPE_WEBSERVER,
                                        (char *) "plugin3", (char *) "lib4" };

    plugin_id_t p_plugin_id = { 0 };

    EXPECT_EQ(0, plugin_manager_reg_plugin(plugin_mng, &param1, &p_plugin_id));
    EXPECT_EQ(0, plugin_manager_reg_plugin(plugin_mng, &param2, &p_plugin_id));
    EXPECT_EQ(0, plugin_manager_reg_plugin(plugin_mng, &param3, &p_plugin_id));

    EXPECT_EQ(0, plugin_manager_update_plugin(plugin_mng, &param_update));

    plugin_info_vec = plugin_manager_get_all_plugins(plugin_mng);
    VECTOR_FOR_EACH(plugin_info_vec, iter)
    {
        plugin_reg_info_t *info = (plugin_reg_info_t *) iterator_get(&iter);

        switch (index) {
        case 4:
            EXPECT_EQ(param_update.adapter_type, info->adapter_type);
            EXPECT_EQ(param_update.plugin_kind, info->plugin_kind);
            EXPECT_STREQ(param_update.plugin_name, info->plugin_name);
            EXPECT_STREQ(param_update.plugin_lib_name, info->plugin_lib_name);
            break;
        default:
            break;
        }
        index += 1;
    }

    vector_free(plugin_info_vec);
    plugin_manager_unreg_all_plugins(plugin_mng);
    plugin_manager_destroy(plugin_mng);
}

void *plugin_manager_thread(void *arg)
{
    plugin_manager_t *plugin_mng      = (plugin_manager_t *) arg;
    vector_t *        plugin_info_vec = { 0 };

    int index = 0;

    plugin_reg_info_t reg_info = { 0 };

    plugin_reg_param_t param1 = { PLUGIN_KIND_SYSTEM, ADAPTER_TYPE_WEBSERVER,
                                  (char *) "plugin1", (char *) "lib1" };
    plugin_reg_param_t param2 = { PLUGIN_KIND_SYSTEM, ADAPTER_TYPE_WEBSERVER,
                                  (char *) "plugin2", (char *) "lib2" };
    plugin_reg_param_t param3 = { PLUGIN_KIND_SYSTEM, ADAPTER_TYPE_WEBSERVER,
                                  (char *) "plugin3", (char *) "lib3" };
    plugin_reg_param_t param_update = { PLUGIN_KIND_SYSTEM,
                                        ADAPTER_TYPE_WEBSERVER,
                                        (char *) "plugin3", (char *) "lib4" };

    plugin_id_t p_plugin_id = { 0 };

    plugin_id_t plugin_id1 = { 0 };
    plugin_id_t plugin_id2 = { 0 };
    plugin_id_t plugin_id3 = { 0 };

    /***plugin_manager_reg_plugin**/
    EXPECT_EQ(0, plugin_manager_reg_plugin(plugin_mng, &param1, &p_plugin_id));
    EXPECT_EQ(0, plugin_manager_reg_plugin(plugin_mng, &param2, &p_plugin_id));
    EXPECT_EQ(0, plugin_manager_reg_plugin(plugin_mng, &param3, &p_plugin_id));

    /**plugin_manager_update_plugin**/
    EXPECT_EQ(0, plugin_manager_update_plugin(plugin_mng, &param_update));

    /**plugin_manager_get_all_plugins**/
    plugin_info_vec = plugin_manager_get_all_plugins(plugin_mng);
    VECTOR_FOR_EACH(plugin_info_vec, iter)
    {
        plugin_reg_info_t *info = (plugin_reg_info_t *) iterator_get(&iter);

        switch (index) {
        case 2:
            plugin_id1 = info->plugin_id;

            EXPECT_EQ(1, info->plugin_kind);
            EXPECT_STREQ("plugin1", info->plugin_name);
            EXPECT_STREQ("lib1", info->plugin_lib_name);

            break;
        case 3:
            plugin_id2 = info->plugin_id;

            EXPECT_EQ(1, info->plugin_kind);
            EXPECT_STREQ("plugin2", info->plugin_name);
            EXPECT_STREQ("lib2", info->plugin_lib_name);
            break;
        case 4:
            plugin_id3 = info->plugin_id;

            EXPECT_EQ(param_update.adapter_type, info->adapter_type);
            EXPECT_EQ(param_update.plugin_kind, info->plugin_kind);
            EXPECT_STREQ(param_update.plugin_name, info->plugin_name);
            EXPECT_STREQ(param_update.plugin_lib_name, info->plugin_lib_name);
            break;
        default:
            break;
        }
        index += 1;
    }

    /**plugin_manager_get_reg_info by plugin_id**/
    EXPECT_EQ(0,
              plugin_manager_get_reg_info(plugin_mng, plugin_id1, &reg_info));
    EXPECT_EQ(0,
              plugin_manager_get_reg_info(plugin_mng, plugin_id2, &reg_info));
    EXPECT_EQ(0,
              plugin_manager_get_reg_info(plugin_mng, plugin_id3, &reg_info));

    /**plugin_manager_get_reg_info_by_name**/
    EXPECT_EQ(0,
              plugin_manager_get_reg_info_by_name(
                  plugin_mng, (char *) "plugin1", &reg_info));
    EXPECT_EQ(0,
              plugin_manager_get_reg_info_by_name(
                  plugin_mng, (char *) "plugin2", &reg_info));
    EXPECT_EQ(0,
              plugin_manager_get_reg_info_by_name(
                  plugin_mng, (char *) "plugin3", &reg_info));

    /**plugin_manager_unreg_plugin**/
    plugin_manager_unreg_plugin(plugin_mng, plugin_id1);
    EXPECT_EQ(-1,
              plugin_manager_get_reg_info(plugin_mng, plugin_id1, &reg_info));

    vector_free(plugin_info_vec);
    plugin_manager_unreg_all_plugins(plugin_mng);
    plugin_manager_destroy(plugin_mng);

    return 0;
}

void *plugin_manager_thread2(void *arg)
{
    plugin_manager_t *plugin_mng2     = (plugin_manager_t *) arg;
    vector_t *        plugin_info_vec = { 0 };

    int index = 0;

    plugin_reg_info_t reg_info = { 0 };

    plugin_reg_param_t param4 = { PLUGIN_KIND_SYSTEM, ADAPTER_TYPE_WEBSERVER,
                                  (char *) "plugin4", (char *) "lib4" };
    plugin_reg_param_t param5 = { PLUGIN_KIND_SYSTEM, ADAPTER_TYPE_WEBSERVER,
                                  (char *) "plugin5", (char *) "lib5" };
    plugin_reg_param_t param6 = { PLUGIN_KIND_SYSTEM, ADAPTER_TYPE_WEBSERVER,
                                  (char *) "plugin6", (char *) "lib6" };
    plugin_reg_param_t param_update2 = { PLUGIN_KIND_SYSTEM,
                                         ADAPTER_TYPE_WEBSERVER,
                                         (char *) "plugin5", (char *) "lib7" };

    plugin_id_t p_plugin_id = { 0 };

    plugin_id_t plugin_id4 = { 0 };
    plugin_id_t plugin_id5 = { 0 };
    plugin_id_t plugin_id6 = { 0 };

    /***plugin_manager_reg_plugin**/
    EXPECT_EQ(0, plugin_manager_reg_plugin(plugin_mng2, &param4, &p_plugin_id));
    EXPECT_EQ(0, plugin_manager_reg_plugin(plugin_mng2, &param5, &p_plugin_id));
    EXPECT_EQ(0, plugin_manager_reg_plugin(plugin_mng2, &param6, &p_plugin_id));

    /**plugin_manager_get_all_plugins**/
    plugin_info_vec = plugin_manager_get_all_plugins(plugin_mng2);
    VECTOR_FOR_EACH(plugin_info_vec, iter)
    {
        plugin_reg_info_t *info = (plugin_reg_info_t *) iterator_get(&iter);

        switch (index) {
        case 2:
            plugin_id4 = info->plugin_id;

            EXPECT_EQ(1, info->plugin_kind);
            EXPECT_STREQ("plugin4", info->plugin_name);
            EXPECT_STREQ("lib4", info->plugin_lib_name);

            break;
        case 3:
            plugin_id5 = info->plugin_id;

            EXPECT_EQ(1, info->plugin_kind);
            EXPECT_STREQ("plugin5", info->plugin_name);
            EXPECT_STREQ("lib5", info->plugin_lib_name);
            break;
        case 4:
            plugin_id6 = info->plugin_id;

            EXPECT_EQ(1, info->plugin_kind);
            EXPECT_STREQ("plugin6", info->plugin_name);
            EXPECT_STREQ("lib6", info->plugin_lib_name);
            break;
        default:
            break;
        }
        index += 1;
    }

    /**plugin_manager_get_reg_info by plugin_id**/
    EXPECT_EQ(0,
              plugin_manager_get_reg_info(plugin_mng2, plugin_id4, &reg_info));
    EXPECT_EQ(0,
              plugin_manager_get_reg_info(plugin_mng2, plugin_id5, &reg_info));
    EXPECT_EQ(0,
              plugin_manager_get_reg_info(plugin_mng2, plugin_id6, &reg_info));

    /**plugin_manager_get_reg_info_by_name**/
    EXPECT_EQ(0,
              plugin_manager_get_reg_info_by_name(
                  plugin_mng2, (char *) "plugin4", &reg_info));
    EXPECT_EQ(0,
              plugin_manager_get_reg_info_by_name(
                  plugin_mng2, (char *) "plugin5", &reg_info));
    EXPECT_EQ(0,
              plugin_manager_get_reg_info_by_name(
                  plugin_mng2, (char *) "plugin6", &reg_info));

    /**plugin_manager_update_plugin**/
    EXPECT_EQ(0, plugin_manager_update_plugin(plugin_mng2, &param_update2));

    /**plugin_manager_unreg_plugin**/
    plugin_manager_unreg_plugin(plugin_mng2, plugin_id4);
    EXPECT_EQ(-1,
              plugin_manager_get_reg_info(plugin_mng2, plugin_id4, &reg_info));

    vector_free(plugin_info_vec);
    plugin_manager_unreg_all_plugins(plugin_mng2);
    plugin_manager_destroy(plugin_mng2);

    return 0;
}

TEST(PluginManagerTest, PluginManagerPthread)
{
    pthread_t         tids[2];
    int               ret1, ret2;
    plugin_manager_t *plugin_mng  = plugin_manager_create();
    plugin_manager_t *plugin_mng2 = plugin_manager_create();

    ret1 = pthread_create(&tids[0], NULL, plugin_manager_thread,
                          (void *) plugin_mng);
    ret2 = pthread_create(&tids[1], NULL, plugin_manager_thread2,
                          (void *) plugin_mng2);

    pthread_join(tids[0], NULL);
    pthread_join(tids[1], NULL);

    EXPECT_EQ(0, ret1);
    EXPECT_EQ(0, ret2);
}
