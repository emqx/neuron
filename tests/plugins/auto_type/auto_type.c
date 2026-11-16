#include <pthread.h>
#include <stdlib.h>

#include "errcodes.h"
#include <neuron.h>

struct neu_plugin {
    neu_plugin_common_t common;
    pthread_mutex_t     lock;
    json_t *            settings;
};

static neu_plugin_t *mock_open(void)
{
    neu_plugin_t *plugin = calloc(1, sizeof(*plugin));
    if (plugin == NULL) {
        return NULL;
    }
    neu_plugin_common_init(&plugin->common);
    pthread_mutex_init(&plugin->lock, NULL);
    plugin->settings = json_object();
    return plugin;
}

static int mock_close(neu_plugin_t *plugin)
{
    json_decref(plugin->settings);
    pthread_mutex_destroy(&plugin->lock);
    free(plugin);
    return 0;
}

static int mock_init(neu_plugin_t *plugin, bool load)
{
    (void) plugin;
    (void) load;
    return 0;
}

static int mock_noop(neu_plugin_t *plugin)
{
    (void) plugin;
    return 0;
}

static bool sample_type(int type)
{
    return (type >= NEU_TYPE_INT8 && type <= NEU_TYPE_LWORD &&
            type != NEU_TYPE_ERROR);
}

static int mock_setting(neu_plugin_t *plugin, const char *config)
{
    json_t *root     = json_loads(config, 0, NULL);
    json_t *settings = json_object_get(root, "params");
    if (!json_is_object(settings)) {
        json_decref(root);
        return NEU_ERR_NODE_SETTING_INVALID;
    }
    json_incref(settings);
    json_decref(root);
    json_t *    type = json_object_get(settings, "type");
    json_t *    mode = json_object_get(settings, "update_mode");
    const char *m    = json_string_value(mode);
    if ((type &&
         (!json_is_integer(type) || !sample_type(json_integer_value(type)))) ||
        (mode &&
         (!m ||
          (strcmp(m, "update") && strcmp(m, "meta") && strcmp(m, "trace") &&
           strcmp(m, "im") && strcmp(m, "im_f_m"))))) {
        json_decref(settings);
        return NEU_ERR_NODE_SETTING_INVALID;
    }
    pthread_mutex_lock(&plugin->lock);
    json_decref(plugin->settings);
    plugin->settings = settings;
    pthread_mutex_unlock(&plugin->lock);
    return 0;
}

static int mock_validate(neu_plugin_t *plugin, neu_datatag_t *tag)
{
    (void) plugin;
    return tag->type == NEU_TYPE_AUTO || sample_type(tag->type)
        ? 0
        : NEU_ERR_TAG_TYPE_NOT_SUPPORT;
}

static int mock_validator(const neu_datatag_t *tag)
{
    (void) tag;
    return 0;
}

static neu_dvalue_t sample(json_t *settings)
{
    json_t *     jtype  = json_object_get(settings, "type");
    json_t *     jvalue = json_object_get(settings, "value");
    int          error = json_integer_value(json_object_get(settings, "error"));
    neu_dvalue_t v     = { .type =
                           jtype ? json_integer_value(jtype) : NEU_TYPE_INT16 };
    int64_t      i     = jvalue ? json_integer_value(jvalue) : 123;
    double       d     = jvalue ? json_number_value(jvalue) : 123;
    const char * s     = json_string_value(jvalue);
    if (!s) {
        s = "sample";
    }
    json_t *quality          = json_object_get(settings, "quality");
    json_t *timestamp        = json_object_get(settings, "timestamp");
    json_t *source_timestamp = json_object_get(settings, "source_timestamp");
    json_t *server_timestamp = json_object_get(settings, "server_timestamp");
    v.attrs.has_quality      = quality != NULL;
    v.attrs.quality          = json_integer_value(quality);
    v.attrs.has_timestamp    = timestamp != NULL;
    v.attrs.timestamp        = json_integer_value(timestamp);
    v.attrs.has_source_timestamp = source_timestamp != NULL;
    v.attrs.source_timestamp     = json_integer_value(source_timestamp);
    v.attrs.has_server_timestamp = server_timestamp != NULL;
    v.attrs.server_timestamp     = json_integer_value(server_timestamp);
    if (error) {
        v.type      = NEU_TYPE_ERROR;
        v.value.i32 = error;
        return v;
    }
    switch (v.type) {
    case NEU_TYPE_INT8:
        v.value.i8 = i;
        break;
    case NEU_TYPE_UINT8:
    case NEU_TYPE_BIT:
        v.value.u8 = i;
        break;
    case NEU_TYPE_INT16:
        v.value.i16 = i;
        break;
    case NEU_TYPE_UINT16:
    case NEU_TYPE_WORD:
        v.value.u16 = i;
        break;
    case NEU_TYPE_INT32:
        v.value.i32 = i;
        break;
    case NEU_TYPE_UINT32:
    case NEU_TYPE_DWORD:
        v.value.u32 = i;
        break;
    case NEU_TYPE_INT64:
        v.value.i64 = i;
        break;
    case NEU_TYPE_UINT64:
    case NEU_TYPE_LWORD:
        v.value.u64 = i;
        break;
    case NEU_TYPE_FLOAT:
        v.value.f32 = d;
        break;
    case NEU_TYPE_DOUBLE:
        v.value.d64 = d;
        break;
    case NEU_TYPE_BOOL:
        v.value.boolean = json_is_true(jvalue);
        break;
    case NEU_TYPE_STRING:
        if (json_is_true(json_object_get(settings, "pointer"))) {
            v.value.ptr.type   = NEU_TYPE_STRING;
            v.value.ptr.length = strlen(s) + 1;
            v.value.ptr.ptr    = (uint8_t *) strdup(s);
            v.type             = NEU_TYPE_PTR;
        } else {
            snprintf(v.value.str, sizeof(v.value.str), "%s", s);
        }
        break;
    case NEU_TYPE_BYTES: {
        size_t n = json_is_array(jvalue) ? json_array_size(jvalue) : 0;
        if (n > UINT8_MAX) {
            n = UINT8_MAX;
        }
        if (json_is_true(json_object_get(settings, "pointer"))) {
            v.value.ptr.type   = NEU_TYPE_BYTES;
            v.value.ptr.length = n;
            v.value.ptr.ptr    = calloc(n ? n : 1, 1);
            for (size_t k = 0; k < n; k++) {
                v.value.ptr.ptr[k] =
                    json_integer_value(json_array_get(jvalue, k));
            }
            v.type = NEU_TYPE_PTR;
        } else {
            v.value.bytes.length = n;
            for (size_t k = 0; k < n; k++) {
                v.value.bytes.bytes[k] =
                    json_integer_value(json_array_get(jvalue, k));
            }
        }
        break;
    }
    default:
        break;
    }
    return v;
}

static int mock_timer(neu_plugin_t *plugin, neu_plugin_group_t *group)
{
    pthread_mutex_lock(&plugin->lock);
    json_t *settings = json_deep_copy(plugin->settings);
    pthread_mutex_unlock(&plugin->lock);
    json_t *hold = json_object_get(settings, "hold");
    if (!hold || !json_is_false(hold)) {
        json_decref(settings);
        return 0;
    }
    const char *mode =
        json_string_value(json_object_get(settings, "update_mode"));
    const adapter_callbacks_t *cb      = plugin->common.adapter_callbacks;
    neu_adapter_t *            adapter = plugin->common.adapter;
    utarray_foreach(group->tags, neu_datatag_t *, tag)
    {
        if (!(tag->attribute & NEU_ATTRIBUTE_READ)) {
            continue;
        }
        neu_dvalue_t v = sample(settings);
        if (mode && !strcmp(mode, "meta")) {
            cb->driver.update_with_meta(adapter, group->group_name, tag->name,
                                        v, NULL, 0);
        } else if (mode && !strcmp(mode, "trace")) {
            cb->driver.update_with_trace(adapter, group->group_name, tag->name,
                                         v, NULL, 0, NULL);
        } else if (mode && !strcmp(mode, "im")) {
            cb->driver.update_im(adapter, group->group_name, tag->name, v, NULL,
                                 0);
        } else if (mode && !strcmp(mode, "im_f_m")) {
            cb->driver.update_im_f_m(adapter, group->group_name, tag->name, v,
                                     NULL, 0);
        } else {
            cb->driver.update(adapter, group->group_name, tag->name, v);
        }
        if (v.type == NEU_TYPE_PTR) {
            free(v.value.ptr.ptr);
        }
    }
    json_decref(settings);
    return 0;
}

static json_t *write_value(neu_type_e type, neu_value_u value)
{
    switch (type) {
    case NEU_TYPE_INT8:
        return json_integer(value.i8);
    case NEU_TYPE_UINT8:
    case NEU_TYPE_BIT:
        return json_integer(value.u8);
    case NEU_TYPE_INT16:
        return json_integer(value.i16);
    case NEU_TYPE_UINT16:
    case NEU_TYPE_WORD:
        return json_integer(value.u16);
    case NEU_TYPE_INT32:
        return json_integer(value.i32);
    case NEU_TYPE_UINT32:
    case NEU_TYPE_DWORD:
        return json_integer(value.u32);
    case NEU_TYPE_INT64:
        return json_integer(value.i64);
    case NEU_TYPE_UINT64:
    case NEU_TYPE_LWORD:
        return json_integer(value.u64);
    case NEU_TYPE_FLOAT:
        return json_real(value.f32);
    case NEU_TYPE_DOUBLE:
        return json_real(value.d64);
    case NEU_TYPE_BOOL:
        return json_boolean(value.boolean);
    case NEU_TYPE_STRING:
        return json_string(value.str);
    case NEU_TYPE_BYTES: {
        json_t *array = json_array();
        for (unsigned i = 0; i < value.bytes.length; i++) {
            json_array_append_new(array, json_integer(value.bytes.bytes[i]));
        }
        return array;
    }
    default:
        return json_null();
    }
}

static int record_write(neu_plugin_t *plugin, neu_datatag_t *tag,
                        neu_value_u value, const char *path)
{
    int error = 0;
    pthread_mutex_lock(&plugin->lock);
    const char *file =
        json_string_value(json_object_get(plugin->settings, "record_path"));
    if (file) {
        FILE *out = fopen(file, "a");
        if (!out) {
            error = NEU_ERR_EINTERNAL;
        } else {
            json_t *record = json_pack("{s:s,s:i,s:s}", "tag", tag->name,
                                       "type", tag->type, "path", path);
            json_object_set_new(record, "value", write_value(tag->type, value));
            if (json_dumpf(record, out, JSON_COMPACT) ||
                fputc('\n', out) == EOF) {
                error = NEU_ERR_EINTERNAL;
            }
            if (fclose(out)) {
                error = NEU_ERR_EINTERNAL;
            }
            json_decref(record);
        }
    }
    pthread_mutex_unlock(&plugin->lock);
    return error;
}

static int mock_write(neu_plugin_t *plugin, void *req, neu_datatag_t *tag,
                      neu_value_u value)
{
    int error = record_write(plugin, tag, value, "single");
    plugin->common.adapter_callbacks->driver.write_response(
        plugin->common.adapter, req, error);
    return 0;
}

static int mock_writes(neu_plugin_t *plugin, void *req, UT_array *tags)
{
    int error = 0;
    utarray_foreach(tags, neu_plugin_tag_value_t *, item)
    {
        int current = record_write(plugin, item->tag, item->value, "batch");
        if (current) {
            error = current;
        }
    }
    plugin->common.adapter_callbacks->driver.write_response(
        plugin->common.adapter, req, error);
    return 0;
}

static const neu_plugin_intf_funs_t functions = {
    .open                 = mock_open,
    .close                = mock_close,
    .init                 = mock_init,
    .uninit               = mock_noop,
    .start                = mock_noop,
    .stop                 = mock_noop,
    .setting              = mock_setting,
    .driver.validate_tag  = mock_validate,
    .driver.tag_validator = mock_validator,
    .driver.group_timer   = mock_timer,
    .driver.group_sync    = mock_timer,
    .driver.write_tag     = mock_write,
    .driver.write_tags    = mock_writes,
};

const neu_plugin_module_t neu_plugin_module = {
    .version         = NEURON_PLUGIN_VER_1_0,
    .schema          = "auto_type",
    .module_name     = "auto_type",
    .module_descr    = "Test-only AUTO type driver",
    .module_descr_zh = "仅测试使用的 AUTO 类型驱动",
    .intf_funs       = &functions,
    .kind            = NEU_PLUGIN_KIND_CUSTOM,
    .type            = NEU_NA_TYPE_DRIVER,
    .display         = true,
    .single          = false,
};
