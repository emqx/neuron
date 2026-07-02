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

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils/ede.h"
#include "utils/neu_path.h"

typedef struct {
    int         object_type;
    const char *area;
    neu_type_e  present_value_type;
} object_map_t;

typedef struct {
    const char *property_id;
    neu_type_e  type;
} property_map_t;

static const object_map_t g_object_map[] = {
    { 0, "AI", NEU_TYPE_FLOAT },   { 1, "AO", NEU_TYPE_FLOAT },
    { 2, "AV", NEU_TYPE_FLOAT },   { 3, "BI", NEU_TYPE_BIT },
    { 4, "BO", NEU_TYPE_BIT },     { 5, "BV", NEU_TYPE_BIT },
    { 8, "DEV", NEU_TYPE_ERROR },  { 13, "MSI", NEU_TYPE_UINT8 },
    { 14, "MSO", NEU_TYPE_UINT8 }, { 19, "MSV", NEU_TYPE_UINT8 },
    { 23, "ACC", NEU_TYPE_UINT8 },
};

static const property_map_t g_property_map[] = {
    { "Object_Name", NEU_TYPE_STRING },
    { "Object_Tyep", NEU_TYPE_UINT8 },
    { "Object_Type", NEU_TYPE_UINT8 },
    { "Description", NEU_TYPE_STRING },
    { "Device_Type", NEU_TYPE_STRING },
    { "Status_Flags", NEU_TYPE_STRING },
    { "Event_State", NEU_TYPE_UINT8 },
    { "Out_Of_Service", NEU_TYPE_BOOL },
    { "Update_Interval", NEU_TYPE_UINT8 },
    { "Min_Pres_Value", NEU_TYPE_FLOAT },
    { "Max_Pres_Value", NEU_TYPE_FLOAT },
    { "Resolution", NEU_TYPE_FLOAT },
    { "COV_Increment", NEU_TYPE_FLOAT },
    { "Time_Delay", NEU_TYPE_UINT8 },
    { "Notification_Class", NEU_TYPE_UINT8 },
    { "Notify_Type", NEU_TYPE_UINT8 },
    { "Units", NEU_TYPE_UINT8 },
    { "High_Limit", NEU_TYPE_FLOAT },
    { "Low_Limit", NEU_TYPE_FLOAT },
    { "Deadband", NEU_TYPE_FLOAT },
    { "Reliability", NEU_TYPE_UINT8 },
    { "Polarity", NEU_TYPE_UINT8 },
    { "System_Status", NEU_TYPE_UINT8 },
    { "Vendor_Name", NEU_TYPE_STRING },
    { "Vendor_Identifier", NEU_TYPE_UINT8 },
    { "Model_Name", NEU_TYPE_STRING },
    { "Firmware_Revision", NEU_TYPE_STRING },
    { "Application_Software_Version", NEU_TYPE_STRING },
    { "Location", NEU_TYPE_STRING },
    { "Protocol_Version", NEU_TYPE_UINT16 },
    { "Protocol_Conformance_Class", NEU_TYPE_UINT8 },
    { "Protocol_Service_Supported", NEU_TYPE_STRING },
    { "Protocol_Object_Types_Supported", NEU_TYPE_STRING },
    { "Serial_Number", NEU_TYPE_STRING },
    { "Max_APDU_Length_Accepted", NEU_TYPE_UINT16 },
    { "Segmentation_Supported", NEU_TYPE_UINT8 },
    { "LOCAL_TIME", NEU_TYPE_STRING },
    { "LOCAL_DATE", NEU_TYPE_STRING },
    { "UTC_Offset", NEU_TYPE_INT8 },
    { "Daylight_Savings_Status", NEU_TYPE_BOOL },
    { "APUD_Segment_Timeout", NEU_TYPE_UINT8 },
    { "APUD_Timeout", NEU_TYPE_UINT16 },
    { "Number_Of_APDU_Retries", NEU_TYPE_UINT8 },
    { "Max_Master", NEU_TYPE_UINT8 },
    { "Max_Info_Frame", NEU_TYPE_UINT8 },
    { "Profile_Name", NEU_TYPE_STRING },
    { "Pulse_Rate", NEU_TYPE_UINT8 },
    { "Scale", NEU_TYPE_FLOAT },
    { "Prescale", NEU_TYPE_FLOAT },
    { "Value_Before_Change", NEU_TYPE_UINT8 },
    { "Value_Change_Time", NEU_TYPE_STRING },
};

static neu_attribute_e commandable_to_attribute(const char *commandable)
{
    if (commandable == NULL || commandable[0] == '\0') {
        return NEU_ATTRIBUTE_READ;
    }

    if (strcmp(commandable, "N") == 0 || strcmp(commandable, "n") == 0 ||
        strcmp(commandable, "0") == 0 || strcmp(commandable, "false") == 0 ||
        strcmp(commandable, "FALSE") == 0) {
        return NEU_ATTRIBUTE_READ;
    }

    return (neu_attribute_e)(NEU_ATTRIBUTE_READ | NEU_ATTRIBUTE_WRITE);
}

static void trim_right(char *s)
{
    size_t len = strlen(s);

    while (len > 0) {
        const char ch = s[len - 1];
        if (ch == '\n' || ch == '\r' || isspace((unsigned char) ch)) {
            s[len - 1] = '\0';
            --len;
            continue;
        }
        break;
    }
}

static int read_field(const char *line, int field_index, char *out,
                      size_t out_size)
{
    const char *start = line;
    const char *end   = line;
    int         idx   = 0;

    if (out_size == 0) {
        return -1;
    }

    while (*end != '\0' && idx < field_index) {
        if (*end == ';') {
            ++idx;
            start = end + 1;
        }
        ++end;
    }

    if (idx != field_index) {
        return -1;
    }

    end = start;
    while (*end != '\0' && *end != ';') {
        ++end;
    }

    size_t copy_len = (size_t)(end - start);
    if (copy_len >= out_size) {
        copy_len = out_size - 1;
    }

    memcpy(out, start, copy_len);
    out[copy_len] = '\0';

    return 0;
}

static const object_map_t *find_object_map(int object_type)
{
    size_t i = 0;
    for (i = 0; i < sizeof(g_object_map) / sizeof(g_object_map[0]); ++i) {
        if (g_object_map[i].object_type == object_type) {
            return &g_object_map[i];
        }
    }
    return NULL;
}

static const char *get_file_name(const char *path)
{
    const char *name = NULL;

    if (path == NULL) {
        return NULL;
    }

    name = strrchr(path, '/');
    if (name == NULL) {
        return path;
    }

    return name + 1;
}

static int push_entry(neu_ede_result_t *result, const char *address,
                      const char *name, neu_type_e data_type,
                      const char *description, neu_attribute_e attribute)
{
    neu_ede_entry_t *new_entries =
        realloc(result->entries, (result->count + 1) * sizeof(neu_ede_entry_t));
    if (new_entries == NULL) {
        return -1;
    }

    result->entries = new_entries;

    if (name == NULL) {
        result->entries[result->count].name[0] = '\0';
    } else {
        snprintf(result->entries[result->count].name,
                 sizeof(result->entries[result->count].name), "%s", name);
    }
    snprintf(result->entries[result->count].address,
             sizeof(result->entries[result->count].address), "%s", address);
    result->entries[result->count].data_type = data_type;
    if (description == NULL) {
        result->entries[result->count].description[0] = '\0';
    } else {
        snprintf(result->entries[result->count].description,
                 sizeof(result->entries[result->count].description), "%s",
                 description);
    }
    result->entries[result->count].attribute = attribute;

    ++result->count;
    return 0;
}

int neu_ede_parse_file(const char *file_path, neu_ede_result_t *result)
{
    FILE *fp                 = NULL;
    char  line[4096]         = { 0 };
    bool  object_header_seen = false;

    if (file_path == NULL || result == NULL) {
        return -1;
    }

    result->entries = NULL;
    result->count   = 0;

    char *safe_path = neu_path_confine(NULL, file_path);
    if (safe_path == NULL) {
        return -1;
    }
    fp = fopen(safe_path, "r");
    free(safe_path);
    if (fp == NULL) {
        return -1;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        char  object_name_buf[256]    = { 0 };
        char  object_type_buf[32]     = { 0 };
        char  object_instance_buf[32] = { 0 };
        char  description_buf[512]    = { 0 };
        char  commandable_buf[32]     = { 0 };
        char  address[128]            = { 0 };
        char *end_ptr                 = NULL;
        long  object_type             = 0;
        long  object_instance         = 0;

        trim_right(line);

        if (line[0] == '\0') {
            continue;
        }

        if (line[0] == '#') {
            if (strstr(line, "object-type") != NULL &&
                strstr(line, "object-instance") != NULL) {
                object_header_seen = true;
            }
            continue;
        }

        if (!object_header_seen) {
            continue;
        }

        if (read_field(line, 2, object_name_buf, sizeof(object_name_buf)) !=
            0) {
            object_name_buf[0] = '\0';
        }
        if (read_field(line, 3, object_type_buf, sizeof(object_type_buf)) !=
            0) {
            continue;
        }
        if (read_field(line, 4, object_instance_buf,
                       sizeof(object_instance_buf)) != 0) {
            continue;
        }
        if (read_field(line, 5, description_buf, sizeof(description_buf)) !=
            0) {
            description_buf[0] = '\0';
        }
        if (read_field(line, 9, commandable_buf, sizeof(commandable_buf)) !=
            0) {
            commandable_buf[0] = '\0';
        }

        errno       = 0;
        object_type = strtol(object_type_buf, &end_ptr, 10);
        if (errno != 0 || end_ptr == object_type_buf || *end_ptr != '\0') {
            continue;
        }

        errno           = 0;
        object_instance = strtol(object_instance_buf, &end_ptr, 10);
        if (errno != 0 || end_ptr == object_instance_buf || *end_ptr != '\0' ||
            object_instance < 0) {
            continue;
        }

        const object_map_t *map = find_object_map((int) object_type);
        if (map == NULL) {
            continue;
        }

        snprintf(address, sizeof(address), "%s%ld", map->area, object_instance);
        if (push_entry(result, address, object_name_buf,
                       map->present_value_type, description_buf,
                       commandable_to_attribute(commandable_buf)) != 0) {
            neu_ede_result_uninit(result);
            fclose(fp);
            return -1;
        }
    }

    fclose(fp);
    return 0;
}

void neu_ede_result_uninit(neu_ede_result_t *result)
{
    if (result == NULL) {
        return;
    }

    free(result->entries);
    result->entries = NULL;
    result->count   = 0;
}

int neu_ede_parse_file_to_tags(const char *file_path, neu_datatag_t **tags,
                               size_t *count)
{
    neu_ede_result_t result       = { 0 };
    neu_datatag_t *  out          = NULL;
    size_t           parsed_count = 0;
    size_t           i            = 0;

    if (tags == NULL || count == NULL) {
        return -1;
    }

    *tags  = NULL;
    *count = 0;

    if (neu_ede_parse_file(file_path, &result) != 0) {
        return -1;
    }

    if (result.count == 0) {
        neu_ede_result_uninit(&result);
        *tags  = NULL;
        *count = 0;
        return 0;
    }

    out = calloc(result.count, sizeof(neu_datatag_t));
    if (out == NULL) {
        neu_ede_result_uninit(&result);
        return -1;
    }

    parsed_count = result.count;

    for (i = 0; i < parsed_count; ++i) {
        out[i].name        = strdup(result.entries[i].name);
        out[i].address     = strdup(result.entries[i].address);
        out[i].attribute   = result.entries[i].attribute;
        out[i].type        = result.entries[i].data_type;
        out[i].description = strdup(result.entries[i].description);
        out[i].unit        = strdup("");

        if (out[i].name == NULL || out[i].address == NULL ||
            out[i].description == NULL || out[i].unit == NULL) {
            neu_ede_tags_uninit(out, parsed_count);
            neu_ede_result_uninit(&result);
            return -1;
        }
    }

    neu_ede_result_uninit(&result);
    *tags  = out;
    *count = parsed_count;
    return 0;
}

void neu_ede_tags_uninit(neu_datatag_t *tags, size_t count)
{
    size_t i = 0;

    if (tags == NULL) {
        return;
    }

    for (i = 0; i < count; ++i) {
        neu_tag_fini(&tags[i]);
    }

    free(tags);
}

void neu_ede_to_msg(char *driver, const char *path, neu_req_add_gtag_t *cmd)
{
    neu_datatag_t *tags      = NULL;
    size_t         count     = 0;
    const char *   base_name = NULL;
    char *         ext       = NULL;

    if (driver == NULL || path == NULL || cmd == NULL) {
        return;
    }

    if (neu_ede_parse_file_to_tags(path, &tags, &count) != 0) {
        return;
    }

    strncpy(cmd->driver, driver, NEU_NODE_NAME_LEN - 1);
    cmd->n_group = 1;
    cmd->groups  = calloc(1, sizeof(neu_gdatatag_t));
    if (cmd->groups == NULL) {
        neu_ede_tags_uninit(tags, count);
        return;
    }

    base_name = get_file_name(path);
    if (base_name == NULL || base_name[0] == '\0') {
        strncpy(cmd->groups[0].group, "ede_import", NEU_GROUP_NAME_LEN - 1);
    } else {
        strncpy(cmd->groups[0].group, base_name, NEU_GROUP_NAME_LEN - 1);
        ext = strrchr(cmd->groups[0].group, '.');
        if (ext != NULL) {
            *ext = '\0';
        }
    }

    cmd->groups[0].interval = 1000;
    cmd->groups[0].n_tag    = (int) count;
    cmd->groups[0].tags     = tags;
}

int neu_ede_format_address(const char *area, uint32_t address,
                           const char *property_id, bool custom,
                           uint32_t custom_id, char *out, size_t out_size)
{
    if (area == NULL || out == NULL || out_size == 0) {
        return -1;
    }

    if (custom) {
        if (snprintf(out, out_size, "%s%u.custom.%u", area, address,
                     custom_id) >= (int) out_size) {
            return -1;
        }
        return 0;
    }

    if (property_id == NULL || property_id[0] == '\0') {
        if (snprintf(out, out_size, "%s%u", area, address) >= (int) out_size) {
            return -1;
        }
        return 0;
    }

    if (strcmp(property_id, "NULL") == 0) {
        return -1;
    }

    if (snprintf(out, out_size, "%s%u.%s", area, address, property_id) >=
        (int) out_size) {
        return -1;
    }

    return 0;
}

neu_type_e neu_ede_get_data_type(const char *area, const char *property_id,
                                 bool custom)
{
    size_t i = 0;

    if (custom) {
        return NEU_TYPE_CUSTOM;
    }

    if (property_id != NULL && property_id[0] != '\0') {
        if (strcmp(property_id, "NULL") == 0) {
            return NEU_TYPE_ERROR;
        }

        for (i = 0; i < sizeof(g_property_map) / sizeof(g_property_map[0]);
             ++i) {
            if (strcmp(property_id, g_property_map[i].property_id) == 0) {
                return g_property_map[i].type;
            }
        }
        return NEU_TYPE_ERROR;
    }

    if (area == NULL) {
        return NEU_TYPE_ERROR;
    }

    for (i = 0; i < sizeof(g_object_map) / sizeof(g_object_map[0]); ++i) {
        if (strcmp(area, g_object_map[i].area) == 0) {
            if (strcmp(area, "DEV") == 0) {
                return NEU_TYPE_ERROR;
            }
            return g_object_map[i].present_value_type;
        }
    }

    return NEU_TYPE_ERROR;
}