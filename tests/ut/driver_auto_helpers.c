#include "driver_auto_helpers.h"
#include "../../src/adapter/driver/driver.c"

bool             sub_filter_err = false;
zlog_category_t *neuron         = NULL;

static void capture_timestamp(void *arg, int64_t timestamp, UT_array *tags,
                              uint32_t interval)
{
    (void) interval;
    ((group_t *) arg)->timestamp = timestamp;
    utarray_free(tags);
}

int auto_test_prepare(neu_type_e resolved, neu_dvalue_t *value, double decimal,
                      double bias, const char *address, int invalidate,
                      bool *valid, neu_type_e *written_type)
{
    neu_adapter_driver_t driver  = { 0 };
    group_t              group   = { .name = "g" };
    neu_datatag_t        tag     = { .name        = "a",
                          .address     = (char *) address,
                          .description = "",
                          .unit        = "",
                          .type        = NEU_TYPE_AUTO,
                          .attribute = NEU_ATTRIBUTE_READ | NEU_ATTRIBUTE_WRITE,
                          .decimal = decimal,
                          .bias    = bias };
    neu_dvalue_t         initial = { .type      = NEU_TYPE_ERROR,
                             .value.i32 = NEU_ERR_PLUGIN_TAG_NOT_READY };
    driver.cache                 = neu_driver_cache_new();
    HASH_ADD_STR(driver.groups, name, &group);
    group.group = neu_group_new("g", 100);
    neu_group_add_tag(group.group, &tag);
    neu_group_change_test(group.group, 0, &group, capture_timestamp);
    neu_driver_cache_add_tag(driver.cache, "g", &tag, initial);
    if (resolved != NEU_TYPE_AUTO) {
        neu_dvalue_t sample = { .type = resolved };
        neu_driver_cache_update(driver.cache, "g", "a", 100, sample, NULL, 0);
    }
    UT_array *types = NULL;
    int       ret   = prepare_write_tag(&driver, &group, &tag, value, &types);
    *written_type   = tag.type;
    if (invalidate == 1) {
        tag.type    = NEU_TYPE_AUTO;
        tag.address = "changed";
        neu_group_update_tag(group.group, &tag);
    } else if (invalidate == 2) {
        tag.type = NEU_TYPE_AUTO;
        neu_driver_cache_add_tag(driver.cache, "g", &tag, initial);
    } else if (invalidate == 3) {
        neu_group_del_tag(group.group, "a");
    }
    *valid = write_types_valid(&driver, types);
    if (types != NULL) {
        utarray_free(types);
    }
    HASH_DEL(driver.groups, &group);
    neu_group_destroy(group.group);
    neu_driver_cache_destroy(driver.cache);
    return ret;
}

int auto_test_metadata(neu_type_e resolved, bool pending, int *count,
                       neu_type_e *type)
{
    neu_adapter_driver_t driver = { 0 };
    group_t              group  = { .name = "g" };
    neu_datatag_t        tag    = { .name        = "a",
                          .address     = "sample",
                          .description = "",
                          .unit        = "",
                          .type        = NEU_TYPE_AUTO,
                          .attribute =
                              NEU_ATTRIBUTE_READ | NEU_ATTRIBUTE_WRITE };
    driver.cache                = neu_driver_cache_new();
    group.group                 = neu_group_new("g", 100);
    HASH_ADD_STR(driver.groups, name, &group);
    neu_group_add_tag(group.group, &tag);
    neu_group_change_test(group.group, 0, &group, capture_timestamp);
    neu_dvalue_t initial = { .type      = NEU_TYPE_ERROR,
                             .value.i32 = NEU_ERR_PLUGIN_TAG_NOT_READY };
    neu_driver_cache_add_tag(driver.cache, "g", &tag, initial);
    if (resolved != NEU_TYPE_AUTO) {
        neu_dvalue_t sample = { .type = resolved };
        neu_driver_cache_update(driver.cache, "g", "a", 100, sample, NULL, 0);
    }
    if (pending) {
        group.timestamp = 0;
    }
    UT_array *tags = NULL;
    int       ret  = neu_adapter_driver_get_value_tag(&driver, "g", &tags);
    *count         = tags ? utarray_len(tags) : 0;
    *type          = tags && utarray_len(tags)
        ? ((neu_datatag_t *) utarray_front(tags))->type
        : NEU_TYPE_AUTO;
    if (tags) {
        utarray_free(tags);
    }
    HASH_DEL(driver.groups, &group);
    neu_group_destroy(group.group);
    neu_driver_cache_destroy(driver.cache);
    return ret;
}

void auto_test_read(neu_dvalue_t sample, double decimal, const char *address,
                    int path, neu_dvalue_t *output, neu_type_e *config_type,
                    neu_type_e *resolved_type)
{
    neu_driver_cache_t *cache   = neu_driver_cache_new();
    neu_datatag_t       tag     = { .name        = "a",
                          .address     = (char *) address,
                          .description = "",
                          .unit        = "",
                          .type        = NEU_TYPE_AUTO,
                          .attribute   = NEU_ATTRIBUTE_READ,
                          .decimal     = decimal };
    neu_dvalue_t        initial = { .type      = NEU_TYPE_ERROR,
                             .value.i32 = NEU_ERR_PLUGIN_TAG_NOT_READY };
    neu_driver_cache_add_tag(cache, "g", &tag, initial);
    neu_driver_cache_update(cache, "g", "a", 100, sample, NULL, 0);
    UT_array *tags, *values;
    utarray_new(tags, neu_tag_get_icd());
    utarray_push_back(tags, &tag);
    if (path == 2) {
        utarray_new(values, neu_resp_tag_value_meta_paginate_icd());
        read_group_paginate(100, 1000, NEU_TAG_CACHE_TYPE_NEVER, cache, "g",
                            tags, values, true);
        neu_resp_tag_value_meta_paginate_t *v = utarray_front(values);
        *output                               = v->value;
        *config_type                          = v->datatag.type;
        *resolved_type                        = v->resolved_type;
        neu_tag_fini(&v->datatag);
    } else {
        utarray_new(values, neu_resp_tag_value_meta_icd());
        if (path == 0) {
            read_group(100, 1000, NEU_TAG_CACHE_TYPE_NEVER, cache, "g", tags,
                       values, true);
        } else {
            read_report_group(100, 1000, NEU_TAG_CACHE_TYPE_NEVER, cache, "g",
                              tags, values);
        }
        neu_resp_tag_value_meta_t *v = utarray_front(values);
        *output                      = v->value;
        *config_type                 = v->datatag.type;
        *resolved_type               = v->resolved_type;
    }
    utarray_free(values);
    utarray_free(tags);
    neu_driver_cache_destroy(cache);
}

void auto_test_snapshot(const auto_test_snapshot_case_t *input, bool paginate,
                        auto_test_snapshot_result_t *output)
{
    neu_adapter_driver_t driver = { 0 };
    group_t              group  = { .name = "g", .driver = &driver };
    neu_datatag_t        tag    = { .name        = "a",
                          .address     = "sample",
                          .description = "",
                          .unit        = "",
                          .type        = input->configured_type,
                          .attribute   = input->write_only
                              ? NEU_ATTRIBUTE_WRITE
                              : NEU_ATTRIBUTE_READ | NEU_ATTRIBUTE_WRITE };
    driver.cache                = neu_driver_cache_new();
    group.group                 = neu_group_new("g", 100);
    HASH_ADD_STR(driver.groups, name, &group);
    neu_group_add_tag(group.group, &tag);
    neu_group_change_test(group.group, 0, &group, capture_timestamp);
    if (!input->missing_cache) {
        neu_dvalue_t initial = { .type      = NEU_TYPE_ERROR,
                                 .value.i32 = NEU_ERR_PLUGIN_TAG_NOT_READY };
        neu_driver_cache_add_tag(driver.cache, "g", &tag, initial);
        if (input->learned_type != NEU_TYPE_AUTO) {
            neu_dvalue_t learned = { .type = input->learned_type };
            neu_driver_cache_update(driver.cache, "g", "a", 100, learned, NULL,
                                    0);
        }
        neu_driver_cache_update(driver.cache, "g", "a", 100, input->sample,
                                NULL, 0);
    }
    if (input->pending) {
        group.timestamp = 0;
    }

    UT_array *listed = NULL;
    neu_adapter_driver_query_tag(&driver, "g", "", &listed);
    neu_resp_tag_info_t *info = utarray_front(listed);
    output->configured_type   = info->tag.type;
    output->listed_type       = info->resolved_type;
    utarray_free(listed);

    UT_array *values;
    utarray_new(values,
                paginate ? neu_resp_tag_value_meta_paginate_icd()
                         : neu_resp_tag_value_meta_icd());
    neu_group_tag_query_t query    = { .current_page = paginate ? 1 : 0,
                                    .page_size    = paginate ? 10 : 0,
                                    .readable     = !paginate };
    read_snapshot_t       snapshot = { .group  = &group,
                                 .values = values,
                                 .now    = input->expired ? 1000000 : 100,
                                 .cache_type = input->expired
                                     ? NEU_TAG_CACHE_TYPE_INTERVAL
                                     : NEU_TAG_CACHE_TYPE_NEVER,
                                 .paginate = paginate,
                                 .error    = input->error };
    neu_group_query_snapshot(group.group, &query, &snapshot, read_snapshot);
    output->read_count = utarray_len(values);
    if (output->read_count) {
        if (paginate) {
            neu_resp_tag_value_meta_paginate_t *value = utarray_front(values);
            output->value                             = value->value;
            output->read_type                         = value->resolved_type;
            neu_tag_fini(&value->datatag);
        } else {
            neu_resp_tag_value_meta_t *value = utarray_front(values);
            output->value                    = value->value;
            output->read_type                = value->resolved_type;
        }
    }
    utarray_free(values);
    HASH_DEL(driver.groups, &group);
    neu_group_destroy(group.group);
    neu_driver_cache_destroy(driver.cache);
}
