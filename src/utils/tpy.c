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
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "utils/log.h"
#include "utils/neu_path.h"
#include "utils/tpy.h"

static int parse_var_entries(xmlNode *section, const char *entry_tag,
                             const char *default_group, xmlNode *datatypes,
                             tpy_t *tpy);

int neu_tpy_parse(const char *path, tpy_t *tpy)
{
    memset(tpy, 0, sizeof(tpy_t));
    char *safe_path = neu_path_confine(NULL, path);
    if (safe_path == NULL) {
        nlog_warn("reject tpy file path outside working dir: %s", path);
        return -1;
    }
    // XML_PARSE_NONET blocks network access for external entities/DTDs (XXE).
    xmlDoc *doc = xmlReadFile(safe_path, NULL, XML_PARSE_NONET);
    free(safe_path);
    if (doc == NULL) {
        nlog_warn("Failed to read tpy file %s", path);
        return -1;
    }
    xmlNode *root = xmlDocGetRootElement(doc);
    if (root == NULL ||
        strcmp((const char *) root->name, "PlcProjectInfo") != 0) {
        nlog_warn("Failed to get root element(PlcProjectInfo)");
        xmlFreeDoc(doc);
        return -1;
    }

    bool     vars_parsed = false;
    xmlNode *datatypes   = NULL;

    for (xmlNode *find = root->children; find != NULL; find = find->next) {
        if (find->type == XML_ELEMENT_NODE &&
            strcmp((const char *) find->name, "DataTypes") == 0) {
            datatypes = find;
            break;
        }
    }

    for (xmlNode *find = root->children; find != NULL; find = find->next) {
        if (find->type == XML_ELEMENT_NODE &&
            strcmp((const char *) find->name, "VarConfigs") == 0 &&
            parse_var_entries(find, "VarConfig", "", datatypes, tpy) == 0) {
            vars_parsed = true;
        }
    }

    for (xmlNode *find = root->children; find != NULL; find = find->next) {
        if (find->type == XML_ELEMENT_NODE &&
            strcmp((const char *) find->name, "Symbols") == 0 &&
            parse_var_entries(find, "Symbol", "Symbols", datatypes, tpy) == 0) {
            vars_parsed = true;
        }
    }

    if (vars_parsed) {
        xmlFreeDoc(doc);
        return 0;
    } else {
        neu_tpy_free(tpy);
        nlog_warn("Failed to parse VarConfigs/Symbols");
        xmlFreeDoc(doc);
        return -1;
    }
}

void neu_tpy_free(tpy_t *tpy)
{
    for (int i = 0; i < tpy->n_statics; i++) {
        if (tpy->statics[i].n_vars > 0) {
            free(tpy->statics[i].vars);
        }
    }

    if (tpy->n_statics > 0) {
        free(tpy->statics);
    }
}

static bool tpy_has_var(tpy_t *tpy, const char *name)
{
    for (int i = 0; i < tpy->n_statics; i++) {
        for (int j = 0; j < tpy->statics[i].n_vars; j++) {
            if (strcmp(tpy->statics[i].vars[j].name, name) == 0) {
                return true;
            }
        }
    }
    return false;
}

static void tpy_add_var(tpy_t *tpy, const char *group, tpy_var_t *tpy_var)
{
    for (int i = 0; i < tpy->n_statics; i++) {
        if (strcmp(tpy->statics[i].name, group) == 0) {
            tpy->statics[i].n_vars += 1;
            tpy->statics[i].vars =
                realloc(tpy->statics[i].vars,
                        tpy->statics[i].n_vars * sizeof(tpy_var_t));
            tpy->statics[i].vars[tpy->statics[i].n_vars - 1] = *tpy_var;
            return;
        }
    }

    tpy->n_statics += 1;
    tpy->statics = realloc(tpy->statics, tpy->n_statics * sizeof(tpy_static_t));
    tpy->statics[tpy->n_statics - 1].n_vars = 1;
    strncpy(tpy->statics[tpy->n_statics - 1].name, group,
            sizeof(tpy->statics[tpy->n_statics - 1].name) - 1);
    tpy->statics[tpy->n_statics - 1].vars    = malloc(sizeof(tpy_var_t));
    tpy->statics[tpy->n_statics - 1].vars[0] = *tpy_var;
}

static bool to_neu_type(const char *type, neu_type_e *out)
{
    if (strcmp(type, "BOOL") == 0) {
        *out = NEU_TYPE_BOOL;
    } else if (strcmp(type, "WORD") == 0) {
        *out = NEU_TYPE_UINT16;
    } else if (strcmp(type, "INT") == 0) {
        *out = NEU_TYPE_INT16;
    } else if (strcmp(type, "USINT") == 0) {
        *out = NEU_TYPE_UINT8;
    } else if (strcmp(type, "BYTE") == 0) {
        *out = NEU_TYPE_UINT8;
    } else if (strcmp(type, "SINT") == 0) {
        *out = NEU_TYPE_INT8;
    } else if (strcmp(type, "UINT") == 0) {
        *out = NEU_TYPE_UINT16;
    } else if (strcmp(type, "UDINT") == 0) {
        *out = NEU_TYPE_UINT32;
    } else if (strcmp(type, "DINT") == 0) {
        *out = NEU_TYPE_INT32;
    } else if (strcmp(type, "DWORD") == 0) {
        *out = NEU_TYPE_UINT32;
    } else if (strcmp(type, "REAL") == 0) {
        *out = NEU_TYPE_FLOAT;
    } else if (strcmp(type, "LREAL") == 0) {
        *out = NEU_TYPE_DOUBLE;
    } else if (strcmp(type, "BIT") == 0) {
        *out = NEU_TYPE_BOOL;
    } else if (strcmp(type, "LINT") == 0) {
        *out = NEU_TYPE_INT64;
    } else if (strcmp(type, "ULINT") == 0) {
        *out = NEU_TYPE_UINT64;
    } else if (strcmp(type, "LWORD") == 0) {
        *out = NEU_TYPE_UINT64;
    } else if (strcmp(type, "TIME") == 0) {
        *out = NEU_TYPE_UINT32;
    } else if (strcmp(type, "DATE") == 0) {
        *out = NEU_TYPE_UINT32;
    } else if (strncmp(type, "STRING", strlen("STRING")) == 0) {
        *out = NEU_TYPE_STRING;
    } else {
        return false;
    }
    return true;
}

static xmlNode *find_datatype(xmlNode *datatypes, const char *type_name)
{
    if (datatypes == NULL) {
        return NULL;
    }

    for (xmlNode *dt = datatypes->children; dt != NULL; dt = dt->next) {
        if (dt->type != XML_ELEMENT_NODE ||
            strcmp((const char *) dt->name, "DataType") != 0) {
            continue;
        }
        for (xmlNode *f = dt->children; f != NULL; f = f->next) {
            if (f->type != XML_ELEMENT_NODE ||
                strcmp((const char *) f->name, "Name") != 0) {
                continue;
            }
            char *content = (char *) xmlNodeGetContent(f);
            bool  match   = content != NULL && strcmp(content, type_name) == 0;
            if (content != NULL) {
                xmlFree(content);
            }
            if (match) {
                return dt;
            }
            break;
        }
    }

    return NULL;
}

#define TPY_MAX_EXPAND_DEPTH 16

static void expand_var(tpy_t *tpy, xmlNode *datatypes, const char *group,
                       const char *name, int64_t igroup, int64_t ioffset,
                       const char *type_name, int64_t bitsize, int depth)
{
    if (depth > TPY_MAX_EXPAND_DEPTH) {
        nlog_warn("tpy: type nesting too deep, skip point: %s", name);
        return;
    }

    neu_type_e neu_type;
    if (to_neu_type(type_name, &neu_type)) {
        if (strlen(name) >= NEU_TAG_NAME_LEN) {
            nlog_warn("tpy: point name too long, skip: %s", name);
            return;
        }
        if (tpy_has_var(tpy, name)) {
            return;
        }
        tpy_var_t tpy_var = { 0 };
        tpy_var.igroup    = igroup;
        tpy_var.ioffset   = ioffset;
        tpy_var.bitsize   = bitsize;
        strncpy(tpy_var.name, name, sizeof(tpy_var.name) - 1);
        strncpy(tpy_var.type, type_name, sizeof(tpy_var.type) - 1);
        tpy_add_var(tpy, group, &tpy_var);
        return;
    }

    xmlNode *dt = find_datatype(datatypes, type_name);
    if (dt == NULL) {
        nlog_warn("tpy: unknown type '%s' for %s, skip", type_name, name);
        return;
    }

    char elem_type[NEU_TAG_NAME_LEN] = { 0 };
    bool has_array                   = false;
    bool has_subitem                 = false;
    bool elem_is_pointer             = false;

    for (xmlNode *f = dt->children; f != NULL; f = f->next) {
        if (f->type != XML_ELEMENT_NODE) {
            continue;
        }
        if (strcmp((const char *) f->name, "Type") == 0) {
            char *content = (char *) xmlNodeGetContent(f);
            if (content != NULL) {
                strncpy(elem_type, content, sizeof(elem_type) - 1);
                xmlFree(content);
            }
            char *pointer = (char *) xmlGetProp(f, (const xmlChar *) "Pointer");
            if (pointer != NULL) {
                elem_is_pointer = true;
                xmlFree(pointer);
            }
        } else if (strcmp((const char *) f->name, "ArrayInfo") == 0) {
            has_array = true;
        } else if (strcmp((const char *) f->name, "SubItem") == 0) {
            has_subitem = true;
        }
    }

    if (has_array) {
        nlog_warn("tpy: array type '%s' not supported for %s, skip", type_name,
                  name);
        return;
    }

    if (elem_is_pointer) {
        nlog_debug("tpy: skip pointer-typed point %s (-> '%s')", name,
                   type_name);
        return;
    }

    if (has_subitem) {
        for (xmlNode *si = dt->children; si != NULL; si = si->next) {
            if (si->type != XML_ELEMENT_NODE ||
                strcmp((const char *) si->name, "SubItem") != 0) {
                continue;
            }

            char    sub_name[NEU_TAG_NAME_LEN] = { 0 };
            char    sub_type[NEU_TAG_NAME_LEN] = { 0 };
            int64_t sub_bitoffs                = 0;
            int64_t sub_bitsize                = 0;
            bool    sub_is_pointer             = false;

            for (xmlNode *f = si->children; f != NULL; f = f->next) {
                if (f->type != XML_ELEMENT_NODE) {
                    continue;
                }
                char *content = (char *) xmlNodeGetContent(f);
                if (content == NULL) {
                    continue;
                }
                if (strcmp((const char *) f->name, "Name") == 0) {
                    strncpy(sub_name, content, sizeof(sub_name) - 1);
                } else if (strcmp((const char *) f->name, "Type") == 0) {
                    strncpy(sub_type, content, sizeof(sub_type) - 1);
                    char *pointer =
                        (char *) xmlGetProp(f, (const xmlChar *) "Pointer");
                    if (pointer != NULL) {
                        sub_is_pointer = true;
                        xmlFree(pointer);
                    }
                } else if (strcmp((const char *) f->name, "BitOffs") == 0) {
                    sub_bitoffs = atoll(content);
                } else if (strcmp((const char *) f->name, "BitSize") == 0) {
                    sub_bitsize = atoll(content);
                }
                xmlFree(content);
            }

            if (strlen(sub_name) == 0 || strlen(sub_type) == 0) {
                continue;
            }

            if (sub_is_pointer) {
                nlog_debug("tpy: skip pointer field %s.%s (-> '%s')", name,
                           sub_name, sub_type);
                continue;
            }

            char full_name[NEU_TAG_NAME_LEN] = { 0 };
            int  n =
                snprintf(full_name, sizeof(full_name), "%s.%s", name, sub_name);
            if (n < 0 || (size_t) n >= sizeof(full_name)) {
                nlog_warn("tpy: point name too long, skip: %s.%s", name,
                          sub_name);
                continue;
            }

            expand_var(tpy, datatypes, group, full_name, igroup,
                       ioffset + sub_bitoffs / 8, sub_type, sub_bitsize,
                       depth + 1);
        }
        return;
    }

    if (strlen(elem_type) > 0 && strcmp(elem_type, type_name) != 0) {
        expand_var(tpy, datatypes, group, name, igroup, ioffset, elem_type,
                   bitsize, depth + 1);
        return;
    }

    nlog_warn("tpy: cannot expand type '%s' for %s, skip", type_name, name);
}

static int parse_var_entries(xmlNode *section, const char *entry_tag,
                             const char *default_group, xmlNode *datatypes,
                             tpy_t *tpy)
{
    xmlNode *var = section->children;
    while (var != NULL) {
        if (var->type == XML_ELEMENT_NODE &&
            strcmp((const char *) var->name, entry_tag) == 0) {
            xmlNode * attr                            = var->children;
            tpy_var_t tpy_var                         = { 0 };
            char      static_name[NEU_GROUP_NAME_LEN] = { 0 };
            char      igroup_str[32]                  = { 0 };
            char      ioffset_str[32]                 = { 0 };
            char      bitsize_str[32]                 = { 0 };
            bool      has_igroup                      = false;
            bool      has_ioffset                     = false;
            bool      is_pointer                      = false;

            while (attr != NULL) {
                if (attr->type == XML_ELEMENT_NODE) {
                    char *content = (char *) xmlNodeGetContent(attr);
                    if (strcmp((const char *) attr->name, "Name") == 0) {
                        char *name_static = (char *) xmlGetProp(
                            attr, (const xmlChar *) "Static");
                        char *name_task = (char *) xmlGetProp(
                            attr, (const xmlChar *) "TaskPrio");

                        snprintf(static_name, sizeof(static_name), "%s_%s",
                                 name_static != NULL ? name_static
                                                     : default_group,
                                 name_task != NULL ? name_task : "");
                        strncpy(tpy_var.name, content,
                                sizeof(tpy_var.name) - 1);
                        xmlFree(name_static);
                        xmlFree(name_task);
                    } else if (strcmp((const char *) attr->name, "Type") == 0) {
                        strncpy(tpy_var.type, content,
                                sizeof(tpy_var.type) - 1);
                        char *pointer = (char *) xmlGetProp(
                            attr, (const xmlChar *) "Pointer");
                        if (pointer != NULL) {
                            is_pointer = true;
                            xmlFree(pointer);
                        }
                    } else if (strcmp((const char *) attr->name, "IGroup") ==
                               0) {
                        strncpy(igroup_str, content, sizeof(igroup_str) - 1);
                        tpy_var.igroup = atoll(igroup_str);
                        has_igroup     = true;
                    } else if (strcmp((const char *) attr->name, "IOffset") ==
                               0) {
                        strncpy(ioffset_str, content, sizeof(ioffset_str) - 1);
                        tpy_var.ioffset = atoll(ioffset_str);
                        has_ioffset     = true;
                    } else if (strcmp((const char *) attr->name, "BitSize") ==
                               0) {
                        strncpy(bitsize_str, content, sizeof(bitsize_str) - 1);
                        tpy_var.bitsize = atoll(bitsize_str);
                    }

                    if (content != NULL) {
                        xmlFree(content);
                    }
                }
                attr = attr->next;
            }

            if (is_pointer) {
                nlog_debug("tpy: skip pointer-typed point %s (-> '%s')",
                           tpy_var.name, tpy_var.type);
            } else if (strlen(tpy_var.name) > 0 && strlen(tpy_var.type) > 0 &&
                       has_igroup && has_ioffset) {
                expand_var(tpy, datatypes, static_name, tpy_var.name,
                           tpy_var.igroup, tpy_var.ioffset, tpy_var.type,
                           tpy_var.bitsize, 0);
            }
        }

        var = var->next;
    }

    return 0;
}

void neu_tpy_to_msg(char *driver, tpy_t *tpy, neu_req_add_gtag_t *cmd)
{
    snprintf(cmd->driver, sizeof(cmd->driver), "%s", driver);
    cmd->n_group = tpy->n_statics;
    cmd->groups  = calloc(cmd->n_group, sizeof(neu_gdatatag_t));

    for (int i = 0; i < cmd->n_group; i++) {
        snprintf(cmd->groups[i].group, sizeof(cmd->groups[i].group), "%s",
                 tpy->statics[i].name);
        cmd->groups[i].interval = 1000;
        cmd->groups[i].n_tag    = tpy->statics[i].n_vars;
        cmd->groups[i].tags =
            calloc(cmd->groups[i].n_tag, sizeof(neu_datatag_t));
        for (int k = 0; k < cmd->groups[i].n_tag; k++) {
            char address[128]           = { 0 };
            cmd->groups[i].tags[k].name = strdup(tpy->statics[i].vars[k].name);
            to_neu_type(tpy->statics[i].vars[k].type,
                        &cmd->groups[i].tags[k].type);
            cmd->groups[i].tags[k].attribute =
                NEU_ATTRIBUTE_READ | NEU_ATTRIBUTE_WRITE;
            if (cmd->groups[i].tags[k].type == NEU_TYPE_STRING) {
                int64_t str_len = tpy->statics[i].vars[k].bitsize / 8;
                if (str_len >= NEU_VALUE_SIZE) {
                    str_len = NEU_VALUE_SIZE - 1;
                }
                snprintf(address, sizeof(address) - 1,
                         "%" PRIi64 ",%" PRIi64 ".%" PRIi64,
                         tpy->statics[i].vars[k].igroup,
                         tpy->statics[i].vars[k].ioffset, str_len);
            } else {
                snprintf(address, sizeof(address) - 1, "%" PRIi64 ",%" PRIi64,
                         tpy->statics[i].vars[k].igroup,
                         tpy->statics[i].vars[k].ioffset);
            }
            cmd->groups[i].tags[k].address     = strdup(address);
            cmd->groups[i].tags[k].description = strdup("");
        }
    }
}