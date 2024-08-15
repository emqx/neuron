/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2022 EMQ Technologies Co., Ltd All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 **/
#include <dlfcn.h>

#include "utils/log.h"
#include "json/neu_json_param.h"

#include "adapter.h"
#include "errcodes.h"

#include "adapter/adapter_internal.h"
#include "adapter/driver/driver_internal.h"
#include "base/msg_internal.h"

#include "manager_internal.h"
#include "template_manager.h"

int neu_manager_add_plugin(neu_manager_t *manager, const char *library)
{
    return neu_plugin_manager_add(manager->plugin_manager, library);
}

int neu_manager_del_plugin(neu_manager_t *manager, const char *plugin)
{
    return neu_plugin_manager_del(manager->plugin_manager, plugin);
}

UT_array *neu_manager_get_plugins(neu_manager_t *manager)
{
    return neu_plugin_manager_get(manager->plugin_manager);
}

int neu_manager_add_node(neu_manager_t *manager, const char *node_name,
                         const char *             plugin_name,
                         neu_node_running_state_e state, bool load)
{
    neu_adapter_t *       adapter      = NULL;
    neu_plugin_instance_t instance     = { 0 };
    neu_adapter_info_t    adapter_info = {
        .name = node_name,
    };
    neu_resp_plugin_info_t info = { 0 };
    int                    ret =
        neu_plugin_manager_find(manager->plugin_manager, plugin_name, &info);

    if (ret != 0) {
        return NEU_ERR_LIBRARY_NOT_FOUND;
    }

    if (info.single) {
        return NEU_ERR_LIBRARY_NOT_ALLOW_CREATE_INSTANCE;
    }

    adapter = neu_node_manager_find(manager->node_manager, node_name);
    if (adapter != NULL) {
        return NEU_ERR_NODE_EXIST;
    }

    ret = neu_plugin_manager_create_instance(manager->plugin_manager, info.name,
                                             &instance);
    if (ret != 0) {
        return NEU_ERR_LIBRARY_FAILED_TO_OPEN;
    }
    adapter_info.handle = instance.handle;
    adapter_info.module = instance.module;

    adapter = neu_adapter_create(&adapter_info, load);
    if (adapter == NULL) {
        return neu_adapter_error();
    }
    neu_node_manager_add(manager->node_manager, adapter);
    neu_adapter_init(adapter, state);

    return NEU_ERR_SUCCESS;
}

int neu_manager_del_node(neu_manager_t *manager, const char *node_name)
{
    neu_adapter_t *adapter =
        neu_node_manager_find(manager->node_manager, node_name);

    if (adapter == NULL) {
        return NEU_ERR_NODE_NOT_EXIST;
    }

    neu_adapter_destroy(adapter);
    neu_subscribe_manager_remove(manager->subscribe_manager, node_name, NULL);
    neu_node_manager_del(manager->node_manager, node_name);
    return NEU_ERR_SUCCESS;
}

UT_array *neu_manager_get_nodes(neu_manager_t *manager, int type,
                                const char *plugin, const char *node)
{
    return neu_node_manager_filter(manager->node_manager, type, plugin, node);
}

int neu_manager_update_node_name(neu_manager_t *manager, const char *node,
                                 const char *new_name)
{
    int ret = 0;
    if (neu_node_manager_is_driver(manager->node_manager, node)) {
        ret = neu_subscribe_manager_update_driver_name(
            manager->subscribe_manager, node, new_name);
    } else {
        ret = neu_subscribe_manager_update_app_name(manager->subscribe_manager,
                                                    node, new_name);
    }
    if (0 == ret) {
        ret =
            neu_node_manager_update_name(manager->node_manager, node, new_name);
    }
    return ret;
}

int neu_manager_update_group_name(neu_manager_t *manager, const char *driver,
                                  const char *group, const char *new_name)
{
    return neu_subscribe_manager_update_group_name(manager->subscribe_manager,
                                                   driver, group, new_name);
}

static inline neu_plugin_instance_t *
new_plugin_instance(neu_plugin_manager_t *plugin_manager, const char *plugin)
{
    neu_plugin_instance_t *inst = calloc(1, sizeof(*inst));
    if (NULL == inst) {
        return NULL;
    }

    if (0 != neu_plugin_manager_create_instance(plugin_manager, plugin, inst)) {
        free(inst);
        return NULL;
    }

    return inst;
}

static inline void free_plugin_instance(neu_plugin_instance_t *inst)
{
    if (inst) {
        dlclose(inst->handle);
        free(inst);
    }
}

int neu_manager_add_template(neu_manager_t *manager, const char *name,
                             const char *plugin, uint16_t n_group,
                             neu_reqresp_template_group_t *groups)
{
    int rv = 0;

    if (!neu_plugin_manager_exists(manager->plugin_manager, plugin)) {
        return NEU_ERR_PLUGIN_NOT_FOUND;
    }

    if (neu_plugin_manager_is_single(manager->plugin_manager, plugin)) {
        return NEU_ERR_PLUGIN_NOT_SUPPORT_TEMPLATE;
    }

    neu_plugin_instance_t *plug_inst =
        new_plugin_instance(manager->plugin_manager, plugin);
    if (NULL == plug_inst) {
        return NEU_ERR_EINTERNAL;
    }

    if (NEU_NA_TYPE_DRIVER != plug_inst->module->type) {
        free_plugin_instance(plug_inst);
        return NEU_ERR_PLUGIN_NOT_SUPPORT_TEMPLATE;
    }

    neu_template_t *tmpl = neu_template_new(name, plugin);
    if (NULL == tmpl) {
        free_plugin_instance(plug_inst);
        return NEU_ERR_EINTERNAL;
    }

    neu_template_set_tag_validator(
        tmpl, plug_inst->module->intf_funs->driver.tag_validator);

    for (int i = 0; i < n_group; ++i) {
        neu_reqresp_template_group_t *grp = &groups[i];
        if (0 !=
            (rv = neu_template_add_group(tmpl, grp->name, grp->interval))) {
            goto error;
        }

        for (int j = 0; j < grp->n_tag; ++j) {
            if (0 !=
                (rv = neu_template_add_tag(tmpl, grp->name, &grp->tags[j]))) {
                goto error;
            }
        }
    }

    rv = neu_template_manager_add(manager->template_manager, tmpl, plug_inst);
    return rv;

error:
    neu_template_free(tmpl);
    free_plugin_instance(plug_inst);
    return rv;
}

int neu_manager_del_template(neu_manager_t *manager, const char *name)
{
    return neu_template_manager_del(manager->template_manager, name);
}

void neu_manager_clear_template(neu_manager_t *manager)
{
    return neu_template_manager_clear(manager->template_manager);
}

static int accumulate_template_group(neu_group_t *grp, void *data)
{
    neu_reqresp_template_t *resp = data;

    // increase group count
    neu_reqresp_template_group_t *resp_grp = &resp->groups[resp->n_group++];

    // copy group name and interval
    strcpy(resp_grp->name, neu_group_get_name(grp));
    resp_grp->interval = neu_group_get_interval(grp);

    // copy tags
    UT_array *tags  = neu_group_get_tag(grp);
    resp_grp->n_tag = utarray_len(tags);
    resp_grp->tags  = utarray_steal(tags);
    utarray_free(tags);

    return 0;
}

int neu_manager_get_template(neu_manager_t *manager, const char *name,
                             neu_resp_get_template_t *resp)
{
    neu_template_t *tmpl =
        neu_template_manager_find(manager->template_manager, name);
    if (NULL == tmpl) {
        return NEU_ERR_TEMPLATE_NOT_FOUND;
    }

    neu_resp_get_template_t data  = { 0 };
    size_t                  count = neu_template_group_num(tmpl);
    if (0 == count) {
        goto end;
    }

    data.groups = calloc(count, sizeof(*data.groups));
    if (NULL == data.groups) {
        return -1;
    }

    int rv =
        neu_template_for_each_group(tmpl, accumulate_template_group, &data);
    if (0 != rv) {
        neu_reqresp_template_fini(&data);
        return rv;
    }

end:
    strncpy(data.name, neu_template_name(tmpl), sizeof(data.name));
    strncpy(data.plugin, neu_template_plugin(tmpl), sizeof(data.plugin));
    *resp = data;

    return 0;
}

static int accumulate_template(neu_template_t *tmpl, void *data)
{
    neu_resp_get_templates_t *resp = data;
    neu_resp_template_info_t *info = &resp->templates[resp->n_templates++];
    strncpy(info->name, neu_template_name(tmpl), sizeof(info->name));
    strncpy(info->plugin, neu_template_plugin(tmpl), sizeof(info->plugin));
    return 0;
}

int neu_manager_get_templates(neu_manager_t *           manager,
                              neu_resp_get_templates_t *resp)
{
    int count = neu_template_manager_count(manager->template_manager);
    if (0 == count) {
        resp->n_templates = 0;
        return 0;
    }

    resp->templates = calloc(count, sizeof(*resp->templates));
    if (NULL == resp->templates) {
        return NEU_ERR_EINTERNAL;
    }

    // guaranteed to success
    neu_template_manager_for_each(manager->template_manager,
                                  accumulate_template, resp);
    return 0;
}

int neu_manager_add_template_group(neu_manager_t *manager,
                                   const char *tmpl_name, const char *group,
                                   uint32_t interval)
{
    neu_template_t *tmpl =
        neu_template_manager_find(manager->template_manager, tmpl_name);
    if (NULL == tmpl) {
        return NEU_ERR_TEMPLATE_NOT_FOUND;
    }

    return neu_template_add_group(tmpl, group, interval);
}

int neu_manager_update_template_group(neu_manager_t *                  manager,
                                      neu_req_update_template_group_t *req)
{
    neu_template_t *tmpl =
        neu_template_manager_find(manager->template_manager, req->tmpl);
    if (NULL == tmpl) {
        return NEU_ERR_TEMPLATE_NOT_FOUND;
    }

    int rv = 0;

    if (strlen(req->new_name) > 0 && 0 != strcmp(req->group, req->new_name)) {
        rv = neu_template_update_group_name(tmpl, req->group, req->new_name);
        if (0 != rv) {
            return rv;
        }
    }

    if (req->interval >= NEU_GROUP_INTERVAL_LIMIT) {
        rv = neu_template_update_group_interval(tmpl, req->new_name,
                                                req->interval);
    }

    return rv;
}

int neu_manager_del_template_group(neu_manager_t *               manager,
                                   neu_req_del_template_group_t *req)
{
    neu_template_t *tmpl =
        neu_template_manager_find(manager->template_manager, req->tmpl);
    if (NULL == tmpl) {
        return NEU_ERR_TEMPLATE_NOT_FOUND;
    }

    return neu_template_del_group(tmpl, req->group);
}

static int accumulate_group_info(neu_group_t *grp, void *data)
{
    UT_array **           vec  = data;
    neu_resp_group_info_t info = { 0 };

    info.interval  = neu_group_get_interval(grp);
    info.tag_count = neu_group_tag_size(grp);
    strncpy(info.name, neu_group_get_name(grp), sizeof(info.name));

    utarray_push_back(*vec, &info);
    return 0;
}

int neu_manager_get_template_group(neu_manager_t *               manager,
                                   neu_req_get_template_group_t *req,
                                   UT_array **                   group_info_p)
{
    neu_template_t *tmpl =
        neu_template_manager_find(manager->template_manager, req->tmpl);
    if (NULL == tmpl) {
        return NEU_ERR_TEMPLATE_NOT_FOUND;
    }

    UT_array *vec = NULL;
    UT_icd    icd = { sizeof(neu_resp_group_info_t), NULL, NULL, NULL };

    utarray_new(vec, &icd);
    if (NULL == vec) {
        return NEU_ERR_EINTERNAL;
    }

    int ret = neu_template_for_each_group(tmpl, accumulate_group_info, &vec);
    if (0 == ret) {
        *group_info_p = vec;
    } else {
        utarray_free(vec);
    }

    return ret;
}

int neu_manager_add_template_tags(neu_manager_t *manager, const char *tmpl_name,
                                  const char *group, uint16_t n_tag,
                                  neu_datatag_t *tags, uint16_t *index_p)
{
    int ret = 0;

    neu_template_t *tmpl =
        neu_template_manager_find(manager->template_manager, tmpl_name);
    if (NULL == tmpl) {
        return NEU_ERR_TEMPLATE_NOT_FOUND;
    }

    int i = 0;
    for (; i < n_tag; ++i) {
        if (0 != (ret = neu_template_add_tag(tmpl, group, &tags[i]))) {
            break;
        }
    }

    if (index_p) {
        *index_p = i;
    }

    return ret;
}

int neu_manager_update_template_tags(neu_manager_t *                manager,
                                     neu_req_update_template_tag_t *req,
                                     uint16_t *                     index_p)
{
    int ret = 0;

    neu_template_t *tmpl =
        neu_template_manager_find(manager->template_manager, req->tmpl);
    if (NULL == tmpl) {
        return NEU_ERR_TEMPLATE_NOT_FOUND;
    }

    int i = 0;
    for (; i < req->n_tag; ++i) {
        if (0 !=
            (ret = neu_template_update_tag(tmpl, req->group, &req->tags[i]))) {
            break;
        }
    }

    if (index_p) {
        *index_p = i;
    }

    return ret;
}

int neu_manager_del_template_tags(neu_manager_t *             manager,
                                  neu_req_del_template_tag_t *req)
{
    int          ret = 0;
    neu_group_t *grp = NULL;

    ret = neu_template_manager_find_group(manager->template_manager, req->tmpl,
                                          req->group, &grp);
    if (0 != ret) {
        return ret;
    }

    int i = 0;
    for (; i < req->n_tag; ++i) {
        // the only possible error is that the tag does not exist
        // in which case we just ignore and continue
        neu_group_del_tag(grp, req->tags[i]);
    }

    return 0;
}

int neu_manager_get_template_tags(neu_manager_t *             manager,
                                  neu_req_get_template_tag_t *req,
                                  UT_array **                 tags_p)
{
    int          ret = 0;
    neu_group_t *grp = NULL;

    ret = neu_template_manager_find_group(manager->template_manager, req->tmpl,
                                          req->group, &grp);
    if (0 != ret) {
        return ret;
    }

    if (strlen(req->name) > 0) {
        *tags_p = neu_group_query_tag(grp, req->name);
    } else {
        *tags_p = neu_group_get_tag(grp);
    }

    return 0;
}

static int add_template_group(neu_group_t *grp, void *data)
{
    neu_adapter_driver_t *driver   = data;
    const char *          name     = neu_group_get_name(grp);
    uint32_t              interval = neu_group_get_interval(grp);

    if (interval < NEU_GROUP_INTERVAL_LIMIT) {
        return NEU_ERR_GROUP_PARAMETER_INVALID;
    }

    int ret = neu_adapter_driver_add_group(driver, name, interval);
    if (0 != ret) {
        return ret;
    }

    UT_array *tags = neu_group_get_tag(grp);
    if (NULL == tags) {
        return NEU_ERR_EINTERNAL;
    }

    ret = neu_adapter_driver_try_add_tag(driver, name, utarray_eltptr(tags, 0),
                                         utarray_len(tags));
    if (0 != ret) {
        utarray_free(tags);
        return ret;
    }

    utarray_foreach(tags, neu_datatag_t *, tag)
    {
        // this invocation is not necessary since we validate when adding tags
        // we keep it here as an act of defensive programming
        ret = neu_adapter_driver_validate_tag(driver, name, tag);
        if (0 != ret) {
            break;
        }

        ret = neu_adapter_driver_add_tag(driver, name, tag);
        if (0 != ret) {
            break;
        }
    }

    // we do not call neu_adapter_driver_try_del_tag here,
    // relying on neu_adapter_driver_uninit to delete the tags

    utarray_free(tags);
    return ret;
}

int neu_manager_instantiate_template(neu_manager_t *manager,
                                     const char *   tmpl_name,
                                     const char *   node_name)
{
    int ret = 0;

    neu_template_t *tmpl =
        neu_template_manager_find(manager->template_manager, tmpl_name);
    if (NULL == tmpl) {
        ret = NEU_ERR_TEMPLATE_NOT_FOUND;
        goto end;
    }

    ret = neu_manager_add_node(manager, node_name, neu_template_plugin(tmpl),
                               false, false);
    if (0 != ret) {
        goto end;
    }

    neu_adapter_t *adapter =
        neu_node_manager_find(manager->node_manager, node_name);
    neu_adapter_driver_t *driver = (neu_adapter_driver_t *) adapter;

    if (adapter->module->type != NEU_NA_TYPE_DRIVER) {
        ret = NEU_ERR_PLUGIN_NOT_SUPPORT_TEMPLATE;
        neu_adapter_uninit(adapter);
        neu_manager_del_node(manager, node_name);
        goto end;
    }

    ret = neu_template_for_each_group(tmpl, add_template_group, driver);
    if (0 != ret) {
        // this will call neu_adapter_driver_try_del_tag to cleanup
        neu_adapter_uninit(adapter);
        neu_manager_del_node(manager, node_name);
    }

end:
    if (0 == ret) {
        nlog_notice("instantiate tmpl:%s node:%s success", tmpl_name,
                    node_name);
    } else {
        nlog_error("instantiate tmpl:%s node:%s ret:%d", tmpl_name, node_name,
                   ret);
    }
    return ret;
}

int neu_manager_instantiate_templates(neu_manager_t *           manager,
                                      neu_req_inst_templates_t *req)
{
    int ret = 0;

    for (int i = 0; i < req->n_inst; ++i) {
        ret = neu_manager_instantiate_template(manager, req->insts[i].tmpl,
                                               req->insts[i].node);
        if (0 != ret) {
            // revert on error, destroy nodes instantiated thus far
            while (--i >= 0) {
                neu_adapter_t *adapter = neu_node_manager_find(
                    manager->node_manager, req->insts[i].node);
                // this will call neu_adapter_driver_try_del_tag to cleanup
                neu_adapter_uninit(adapter);
                neu_manager_del_node(manager, req->insts[i].node);
            }
            break;
        }
    }

    return ret;
}

UT_array *neu_manager_get_driver_group(neu_manager_t *manager)
{
    UT_array *drivers =
        neu_node_manager_get(manager->node_manager, NEU_NA_TYPE_DRIVER);
    UT_array *driver_groups = NULL;
    UT_icd    icd = { sizeof(neu_resp_driver_group_info_t), NULL, NULL, NULL };

    utarray_new(driver_groups, &icd);

    utarray_foreach(drivers, neu_resp_node_info_t *, driver)
    {
        neu_adapter_t *adapter =
            neu_node_manager_find(manager->node_manager, driver->node);
        UT_array *groups =
            neu_adapter_driver_get_group((neu_adapter_driver_t *) adapter);

        utarray_foreach(groups, neu_resp_group_info_t *, g)
        {
            neu_resp_driver_group_info_t dg = { 0 };

            strcpy(dg.driver, driver->node);
            strcpy(dg.group, g->name);
            dg.interval  = g->interval;
            dg.tag_count = g->tag_count;

            utarray_push_back(driver_groups, &dg);
        }

        utarray_free(groups);
    }

    utarray_free(drivers);

    return driver_groups;
}

static inline int manager_subscribe(neu_manager_t *manager, const char *app,
                                    const char *driver, const char *group,
                                    const char *params)
{
    int                ret  = NEU_ERR_SUCCESS;
    struct sockaddr_in addr = { 0 };
    neu_adapter_t *    adapter =
        neu_node_manager_find(manager->node_manager, driver);

    if (adapter == NULL) {
        return NEU_ERR_NODE_NOT_EXIST;
    }

    ret =
        neu_adapter_driver_group_exist((neu_adapter_driver_t *) adapter, group);
    if (ret != NEU_ERR_SUCCESS) {
        return ret;
    }

    addr = neu_node_manager_get_addr(manager->node_manager, app);
    return neu_subscribe_manager_sub(manager->subscribe_manager, driver, app,
                                     group, params, addr);
}

int neu_manager_subscribe(neu_manager_t *manager, const char *app,
                          const char *driver, const char *group,
                          const char *params, uint16_t *app_port)
{
    if (neu_node_manager_is_monitor(manager->node_manager, app)) {
        // filter out monitor node
        return NEU_ERR_NODE_NOT_ALLOW_SUBSCRIBE;
    }

    neu_adapter_t *adapter = neu_node_manager_find(manager->node_manager, app);

    if (adapter == NULL) {
        return NEU_ERR_NODE_NOT_EXIST;
    }

    *app_port = neu_adapter_trans_data_port(adapter);

    // guard against empty mqtt topic parameter
    // this is not an elegant solution due to the current architecture
    if (params && 0 == strcmp(adapter->module->module_name, "MQTT")) {
        neu_json_elem_t elem = { .name      = "topic",
                                 .t         = NEU_JSON_STR,
                                 .v.val_str = NULL };
        int             ret  = neu_parse_param(params, NULL, 1, &elem);
        if (ret != 0 || (elem.v.val_str && 0 == strlen(elem.v.val_str))) {
            free(elem.v.val_str);
            return NEU_ERR_MQTT_SUBSCRIBE_FAILURE;
        }
        free(elem.v.val_str);
    }

    if (NEU_NA_TYPE_APP != neu_adapter_get_type(adapter)) {
        return NEU_ERR_NODE_NOT_ALLOW_SUBSCRIBE;
    }

    return manager_subscribe(manager, app, driver, group, params);
}

int neu_manager_update_subscribe(neu_manager_t *manager, const char *app,
                                 const char *driver, const char *group,
                                 const char *params)
{
    return neu_subscribe_manager_update_params(manager->subscribe_manager, app,
                                               driver, group, params);
}

int neu_manager_send_subscribe(neu_manager_t *manager, const char *app,
                               const char *driver, const char *group,
                               uint16_t app_port, const char *params)
{
    neu_req_subscribe_t cmd = { 0 };
    strcpy(cmd.app, app);
    strcpy(cmd.driver, driver);
    strcpy(cmd.group, group);
    cmd.port = app_port;

    if (params && NULL == (cmd.params = strdup(params))) {
        return NEU_ERR_EINTERNAL;
    }

    neu_msg_t *msg = neu_msg_new(NEU_REQ_SUBSCRIBE_GROUP, NULL, &cmd);
    if (NULL == msg) {
        free(cmd.params);
        return NEU_ERR_EINTERNAL;
    }
    neu_reqresp_head_t *header = neu_msg_get_header(msg);
    strcpy(header->sender, "manager");
    strcpy(header->receiver, app);

    struct sockaddr_in addr =
        neu_node_manager_get_addr(manager->node_manager, app);

    int ret = neu_send_msg_to(manager->server_fd, &addr, msg);
    if (0 != ret) {
        nlog_warn("send %s to %s app failed",
                  neu_reqresp_type_string(NEU_REQ_SUBSCRIBE_GROUP), app);
        free(cmd.params);
        neu_msg_free(msg);
    } else {
        nlog_notice("send %s to %s app",
                    neu_reqresp_type_string(NEU_REQ_SUBSCRIBE_GROUP), app);
    }
    cmd.params = NULL;

    msg = neu_msg_new(NEU_REQ_SUBSCRIBE_GROUP, NULL, &cmd);
    if (NULL == msg) {
        return NEU_ERR_EINTERNAL;
    }
    header = neu_msg_get_header(msg);
    strcpy(header->sender, "manager");
    strcpy(header->receiver, driver);
    addr = neu_node_manager_get_addr(manager->node_manager, driver);

    ret = neu_send_msg_to(manager->server_fd, &addr, msg);
    if (0 != ret) {
        nlog_warn("send %s to %s driver failed",
                  neu_reqresp_type_string(NEU_REQ_SUBSCRIBE_GROUP), driver);
        neu_msg_free(msg);
    } else {
        nlog_notice("send %s to %s driver",
                    neu_reqresp_type_string(NEU_REQ_SUBSCRIBE_GROUP), driver);
    }

    return 0;
}

int neu_manager_unsubscribe(neu_manager_t *manager, const char *app,
                            const char *driver, const char *group)
{
    return neu_subscribe_manager_unsub(manager->subscribe_manager, driver, app,
                                       group);
}

UT_array *neu_manager_get_sub_group(neu_manager_t *manager, const char *app)
{
    return neu_subscribe_manager_get(manager->subscribe_manager, app);
}

UT_array *neu_manager_get_sub_group_deep_copy(neu_manager_t *manager,
                                              const char *   app)
{
    UT_array *subs = neu_subscribe_manager_get(manager->subscribe_manager, app);

    utarray_foreach(subs, neu_resp_subscribe_info_t *, sub)
    {
        if (sub->params) {
            sub->params = strdup(sub->params);
        }
    }

    // set vector element destructor
    subs->icd.dtor = (void (*)(void *)) neu_resp_subscribe_info_fini;

    return subs;
}

int neu_manager_add_ndriver_map(neu_manager_t *manager, const char *ndriver,
                                const char *driver, const char *group)
{
    neu_adapter_t *adapter =
        neu_node_manager_find(manager->node_manager, ndriver);

    if (adapter == NULL) {
        return NEU_ERR_NODE_NOT_EXIST;
    }

    if (NEU_NA_TYPE_NDRIVER != neu_adapter_get_type(adapter)) {
        return NEU_ERR_NODE_NOT_ALLOW_MAP;
    }

    return manager_subscribe(manager, ndriver, driver, group, NULL);
}

int neu_manager_del_ndriver_map(neu_manager_t *manager, const char *ndriver,
                                const char *driver, const char *group)
{
    return neu_manager_unsubscribe(manager, ndriver, driver, group);
}

int neu_manager_get_ndriver_maps(neu_manager_t *manager, const char *ndriver,
                                 UT_array **result)
{
    neu_adapter_t *adapter =
        neu_node_manager_find(manager->node_manager, ndriver);

    if (adapter == NULL) {
        return NEU_ERR_NODE_NOT_EXIST;
    } else if (NEU_NA_TYPE_NDRIVER != neu_adapter_get_type(adapter)) {
        return NEU_ERR_NODE_NOT_ALLOW_MAP;
    }

    *result = neu_subscribe_manager_get_ndriver_maps(manager->subscribe_manager,
                                                     ndriver);
    return 0;
}

int neu_manager_get_node_info(neu_manager_t *manager, const char *name,
                              neu_persist_node_info_t *info)
{
    neu_adapter_t *adapter = neu_node_manager_find(manager->node_manager, name);

    if (adapter != NULL) {
        info->name        = strdup(name);
        info->type        = adapter->module->type;
        info->plugin_name = strdup(adapter->module->module_name);
        info->state       = adapter->state;
        return 0;
    }

    return -1;
}
