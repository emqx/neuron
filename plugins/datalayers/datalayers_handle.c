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

#include <jansson.h>

#include "errcodes.h"
#include "plugin.h"
#include "type.h"

#include "datalayers_handle.h"
#include "datalayers_plugin.h"
#include "datalayers_plugin_intf.h"

void task_queue_init(task_queue_t *queue)
{
    queue->head     = NULL;
    queue->tail     = NULL;
    queue->size     = 0;
    queue->max_size = 0;
}

static inline db_write_task_t *task_new()
{
    db_write_task_t *task = malloc(sizeof(db_write_task_t));
    if (!task) {
        return NULL;
    }
    memset(task, 0, sizeof(db_write_task_t));

    return task;
}

static inline void task_free(db_write_task_t *task)
{
    if (task) {
        utarray_free(task->int_tags);
        utarray_free(task->float_tags);
        utarray_free(task->bool_tags);
        utarray_free(task->string_tags);
        free(task);
    }
}

void tasks_free(task_queue_t *queue)
{
    db_write_task_t *task = queue->head;
    while (task) {
        db_write_task_t *next = task->next;
        task_free(task);
        task = next;
    }
    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
}

void task_queue_push(neu_plugin_t *plugin, db_write_task_t *task)
{
    if (plugin->task_queue.size >= MAX_QUEUE_SIZE) {
        db_write_task_t *old_task = plugin->task_queue.head;
        plugin->task_queue.head   = plugin->task_queue.head->next;
        task_free(old_task);
        plugin->task_queue.size--;
        NEU_PLUGIN_UPDATE_DISCARDED_MSGS_METRIC(plugin, 1);
    }

    if (plugin->task_queue.tail) {
        plugin->task_queue.tail->next = task;
    }
    plugin->task_queue.tail = task;
    if (plugin->task_queue.head == NULL) {
        plugin->task_queue.head = task;
    }
    plugin->task_queue.size++;

    if (plugin->task_queue.size > plugin->task_queue.max_size) {
        plugin->task_queue.max_size = plugin->task_queue.size;
        NEU_PLUGIN_UPDATE_MAX_CACHED_QUEUE_SIZE_METRIC(
            plugin, plugin->task_queue.max_size);
    }
    NEU_PLUGIN_UPDATE_CACHED_QUEUE_SIZE_METRIC(plugin, plugin->task_queue.size);
}

db_write_task_t *task_queue_pop(neu_plugin_t *plugin, task_queue_t *queue)
{
    if (queue->size == 0) {
        return NULL;
    }

    db_write_task_t *task = queue->head;
    queue->head           = queue->head->next;
    if (queue->head == NULL) {
        queue->tail = NULL;
    }
    queue->size--;

    NEU_PLUGIN_UPDATE_CACHED_QUEUE_SIZE_METRIC(plugin, plugin->task_queue.size);

    return task;
}

static void db_write_task_cb(db_write_task_t *task, neu_plugin_t *plugin)
{
    int ret = 0;
    if (utarray_len(task->int_tags) > 0) {
        ret = client_insert(plugin->client, INT_TYPE,
                            utarray_eltptr(task->int_tags, 0),
                            utarray_len(task->int_tags));
    }

    if (utarray_len(task->float_tags) > 0) {
        ret = client_insert(plugin->client, FLOAT_TYPE,
                            utarray_eltptr(task->float_tags, 0),
                            utarray_len(task->float_tags));
    }

    if (utarray_len(task->bool_tags) > 0) {
        ret = client_insert(plugin->client, BOOL_TYPE,
                            utarray_eltptr(task->bool_tags, 0),
                            utarray_len(task->bool_tags));
    }

    if (utarray_len(task->string_tags) > 0) {
        ret = client_insert(plugin->client, STRING_TYPE,
                            utarray_eltptr(task->string_tags, 0),
                            utarray_len(task->string_tags));
    }

    if (ret != 0) {
        plog_error(plugin, "insert data to datalayers failed, disconnected");
        if (plugin->client) {
            client_destroy(plugin->client);
            plugin->client = NULL;
        }
    }
}

void db_write_task_consumer(neu_plugin_t *plugin)
{
    db_write_task_t *task = NULL;

    while (1) {
        task = task_queue_pop(plugin, &plugin->task_queue);
        if (task) {
            db_write_task_cb(task, plugin);

            task_free(task);
        } else {
            usleep(1000);
        }
    }
}

static void tag_array_copy(void *dst, const void *src)
{
    const datatag *src_tag = (const datatag *) src;
    datatag *      dst_tag = (datatag *) dst;

    dst_tag->node_name = src_tag->node_name ? strdup(src_tag->node_name) : NULL;
    dst_tag->group_name =
        src_tag->group_name ? strdup(src_tag->group_name) : NULL;
    dst_tag->tag = src_tag->tag ? strdup(src_tag->tag) : NULL;

    dst_tag->value_type = src_tag->value_type;

    switch (src_tag->value_type) {
    case STRING_TYPE:
        dst_tag->value.string_value = src_tag->value.string_value
            ? strdup(src_tag->value.string_value)
            : NULL;
        break;
    case INT_TYPE:
        dst_tag->value.int_value = src_tag->value.int_value;
        break;
    case FLOAT_TYPE:
        dst_tag->value.float_value = src_tag->value.float_value;
        break;
    case BOOL_TYPE:
        dst_tag->value.bool_value = src_tag->value.bool_value;
        break;
    default:
        break;
    }
}

static void tag_array_free(void *_elt)
{
    datatag *elt = (datatag *) _elt;

    free((void *) elt->node_name);
    free((void *) elt->group_name);
    free((void *) elt->tag);

    if (elt->value_type == STRING_TYPE) {
        free((void *) elt->value.string_value);
    }
}

static void tag_array_init(void *_elt)
{
    datatag *elt = (datatag *) _elt;
    memset(elt, 0, sizeof(datatag));
}

static UT_icd ut_datatag_icd = { sizeof(datatag), tag_array_init,
                                 tag_array_copy, tag_array_free };

void process_array_to_json_string(json_t *                   array,
                                  neu_resp_tag_value_meta_t *tag_meta,
                                  UT_array *                 string_tags,
                                  neu_reqresp_trans_data_t * trans_data)
{
    switch (tag_meta->value.type) {
    case NEU_TYPE_ARRAY_INT8:
        for (size_t i = 0; i < tag_meta->value.value.i8s.length; ++i) {
            json_array_append_new(
                array, json_integer(tag_meta->value.value.i8s.i8s[i]));
        }
        break;

    case NEU_TYPE_ARRAY_UINT8:
        for (size_t i = 0; i < tag_meta->value.value.u8s.length; ++i) {
            json_array_append_new(
                array, json_integer(tag_meta->value.value.u8s.u8s[i]));
        }
        break;

    case NEU_TYPE_ARRAY_INT16:
        for (size_t i = 0; i < tag_meta->value.value.i16s.length; ++i) {
            json_array_append_new(
                array, json_integer(tag_meta->value.value.i16s.i16s[i]));
        }
        break;

    case NEU_TYPE_ARRAY_UINT16:
        for (size_t i = 0; i < tag_meta->value.value.u16s.length; ++i) {
            json_array_append_new(
                array, json_integer(tag_meta->value.value.u16s.u16s[i]));
        }
        break;

    case NEU_TYPE_ARRAY_INT32:
        for (size_t i = 0; i < tag_meta->value.value.i32s.length; ++i) {
            json_array_append_new(
                array, json_integer(tag_meta->value.value.i32s.i32s[i]));
        }
        break;

    case NEU_TYPE_ARRAY_UINT32:
        for (size_t i = 0; i < tag_meta->value.value.u32s.length; ++i) {
            json_array_append_new(
                array, json_integer(tag_meta->value.value.u32s.u32s[i]));
        }
        break;

    case NEU_TYPE_ARRAY_INT64:
        for (size_t i = 0; i < tag_meta->value.value.i64s.length; ++i) {
            json_array_append_new(
                array, json_integer(tag_meta->value.value.i64s.i64s[i]));
        }
        break;

    case NEU_TYPE_ARRAY_UINT64:
        for (size_t i = 0; i < tag_meta->value.value.u64s.length; ++i) {
            json_array_append_new(
                array, json_integer(tag_meta->value.value.u64s.u64s[i]));
        }
        break;

    case NEU_TYPE_ARRAY_FLOAT:
        for (size_t i = 0; i < tag_meta->value.value.f32s.length; ++i) {
            json_array_append_new(
                array, json_real(tag_meta->value.value.f32s.f32s[i]));
        }
        break;

    case NEU_TYPE_ARRAY_DOUBLE:
        for (size_t i = 0; i < tag_meta->value.value.f64s.length; ++i) {
            json_array_append_new(
                array, json_real(tag_meta->value.value.f64s.f64s[i]));
        }
        break;

    case NEU_TYPE_ARRAY_STRING:
        for (size_t i = 0; i < tag_meta->value.value.strs.length; ++i) {
            json_array_append_new(
                array, json_string(tag_meta->value.value.strs.strs[i]));
        }
        break;

    case NEU_TYPE_ARRAY_BOOL:
        for (size_t i = 0; i < tag_meta->value.value.bools.length; ++i) {
            json_array_append_new(
                array, json_boolean(tag_meta->value.value.bools.bools[i]));
        }
        break;

    case NEU_TYPE_BYTES:
        for (size_t i = 0; i < tag_meta->value.value.bytes.length; ++i) {
            json_array_append_new(
                array, json_integer(tag_meta->value.value.bytes.bytes[i]));
        }
        break;

    default:
        break;
    }

    char *json_str = json_dumps(array, JSON_COMPACT);
    json_decref(array);

    datatag tag = { trans_data->driver,
                    trans_data->group,
                    tag_meta->tag,
                    { .string_value = json_str },
                    STRING_TYPE };
    utarray_push_back(string_tags, &tag);

    free(json_str);
}

int handle_trans_data(neu_plugin_t *            plugin,
                      neu_reqresp_trans_data_t *trans_data)
{
    int rv = 0;

    if (NULL == plugin->client) {
        return NEU_ERR_DATALAYERS_IS_NULL;
    }

    if (plugin->common.link_state != NEU_NODE_LINK_STATE_CONNECTED) {
        return NEU_ERR_PLUGIN_NOT_RUNNING;
    }

    const route_entry_t *route = route_tbl_get(
        &plugin->route_tbl, trans_data->driver, trans_data->group);
    if (NULL == route) {
        plog_error(plugin, "no route for driver:%s group:%s",
                   trans_data->driver, trans_data->group);
        return NEU_ERR_GROUP_NOT_SUBSCRIBE;
    }

    UT_array *int_tags = NULL, *float_tags = NULL, *bool_tags = NULL,
             *string_tags = NULL;
    utarray_new(int_tags, &ut_datatag_icd);
    utarray_new(float_tags, &ut_datatag_icd);
    utarray_new(bool_tags, &ut_datatag_icd);
    utarray_new(string_tags, &ut_datatag_icd);

    utarray_foreach(trans_data->tags, neu_resp_tag_value_meta_t *, tag_meta)
    {
        if (tag_meta->value.type == NEU_TYPE_ERROR) {
            continue;
        }

        switch (tag_meta->value.type) {
        case NEU_TYPE_BIT:
        case NEU_TYPE_INT8:
        case NEU_TYPE_UINT8:
        case NEU_TYPE_INT16:
        case NEU_TYPE_UINT16:
        case NEU_TYPE_INT32:
        case NEU_TYPE_UINT32:
        case NEU_TYPE_INT64:
        case NEU_TYPE_UINT64: {
            datatag tag = { trans_data->driver,
                            trans_data->group,
                            tag_meta->tag,
                            { .int_value = tag_meta->value.value.i64 },
                            INT_TYPE };
            utarray_push_back(int_tags, &tag);
        } break;
        case NEU_TYPE_FLOAT:
        case NEU_TYPE_DOUBLE: {
            datatag tag = { trans_data->driver,
                            trans_data->group,
                            tag_meta->tag,
                            { .float_value = tag_meta->value.value.d64 },
                            FLOAT_TYPE };
            utarray_push_back(float_tags, &tag);
        } break;
        case NEU_TYPE_BOOL: {
            datatag tag = { trans_data->driver,
                            trans_data->group,
                            tag_meta->tag,
                            { .bool_value = tag_meta->value.value.boolean },
                            BOOL_TYPE };
            utarray_push_back(bool_tags, &tag);
        } break;
        case NEU_TYPE_STRING:
        case NEU_TYPE_DATA_AND_TIME:
        case NEU_TYPE_TIME: {
            datatag tag = { trans_data->driver,
                            trans_data->group,
                            tag_meta->tag,
                            { .string_value = tag_meta->value.value.str },
                            STRING_TYPE };
            utarray_push_back(string_tags, &tag);
        } break;
        case NEU_TYPE_BYTES:
        case NEU_TYPE_ARRAY_BOOL:
        case NEU_TYPE_ARRAY_INT8:
        case NEU_TYPE_ARRAY_UINT8:
        case NEU_TYPE_ARRAY_INT16:
        case NEU_TYPE_ARRAY_UINT16:
        case NEU_TYPE_ARRAY_INT32:
        case NEU_TYPE_ARRAY_UINT32:
        case NEU_TYPE_ARRAY_INT64:
        case NEU_TYPE_ARRAY_UINT64:
        case NEU_TYPE_ARRAY_FLOAT:
        case NEU_TYPE_ARRAY_DOUBLE: {
            json_t *array = json_array();
            process_array_to_json_string(array, tag_meta, string_tags,
                                         trans_data);
        } break;
        default:
            break;
        }
    }

    db_write_task_t *task = task_new();
    task->int_tags        = int_tags;
    task->float_tags      = float_tags;
    task->bool_tags       = bool_tags;
    task->string_tags     = string_tags;

    task_queue_push(plugin, task);

    return rv;
}

int handle_subscribe_group(neu_plugin_t *plugin, neu_req_subscribe_t *sub_info)
{
    int rv = 0;

    rv = route_tbl_add_new(&plugin->route_tbl, sub_info->driver,
                           sub_info->group);
    if (0 != rv) {
        plog_error(plugin, "route driver:%s group:%s fail, `%s`",
                   sub_info->driver, sub_info->group, sub_info->params);
        goto end;
    }

    plog_notice(plugin, "route driver:%s group:%s", sub_info->driver,
                sub_info->group);

end:
    free(sub_info->params);
    return rv;
}

int handle_unsubscribe_group(neu_plugin_t *         plugin,
                             neu_req_unsubscribe_t *unsub_info)
{
    route_tbl_del(&plugin->route_tbl, unsub_info->driver, unsub_info->group);
    plog_notice(plugin, "del route driver:%s group:%s", unsub_info->driver,
                unsub_info->group);
    return 0;
}

int handle_del_group(neu_plugin_t *plugin, neu_req_del_group_t *req)
{
    route_tbl_del(&plugin->route_tbl, req->driver, req->group);
    plog_notice(plugin, "del route driver:%s group:%s", req->driver,
                req->group);
    return 0;
}

int handle_update_group(neu_plugin_t *plugin, neu_req_update_group_t *req)
{
    route_tbl_update_group(&plugin->route_tbl, req->driver, req->group,
                           req->new_name);
    plog_notice(plugin, "update route driver:%s group:%s to %s", req->driver,
                req->group, req->new_name);
    return 0;
}

int handle_update_driver(neu_plugin_t *plugin, neu_req_update_node_t *req)
{
    route_tbl_update_driver(&plugin->route_tbl, req->node, req->new_name);
    plog_notice(plugin, "update route driver:%s to %s", req->node,
                req->new_name);
    return 0;
}

int handle_del_driver(neu_plugin_t *plugin, neu_reqresp_node_deleted_t *req)
{
    route_tbl_del_driver(&plugin->route_tbl, req->node);
    plog_notice(plugin, "delete route driver:%s", req->node);
    return 0;
}