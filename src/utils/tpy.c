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
#include "utils/tpy.h"

static int parse_vars(xmlNode *xml_ied, tpy_t *tpy);

int neu_tpy_parse(const char *path, tpy_t *tpy)
{
    memset(tpy, 0, sizeof(tpy_t));
    xmlDoc *doc = xmlReadFile(path, NULL, 0);
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

    xmlNode *find        = root->children;
    bool     vars_parsed = false;

    while (find != NULL) {
        if (find->type == XML_ELEMENT_NODE &&
            strcmp((const char *) find->name, "VarConfigs") == 0) {
            int ret = parse_vars(find, tpy);
            if (ret == 0) {
                vars_parsed = true;
            }
        }

        find = find->next;
    }

    if (vars_parsed) {
        xmlFreeDoc(doc);
        return 0;
    } else {
        neu_tpy_free(tpy);
        nlog_warn("Failed to parse VarConfigs");
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

static int parse_vars(xmlNode *xml_ied, tpy_t *tpy)
{
    xmlNode *var = xml_ied->children;
    while (var != NULL) {
        if (var->type == XML_ELEMENT_NODE &&
            strcmp((const char *) var->name, "VarConfig") == 0) {
            xmlNode * attr                            = var->children;
            tpy_var_t tpy_var                         = { 0 };
            char      static_name[NEU_GROUP_NAME_LEN] = { 0 };
            char      igroup_str[32]                  = { 0 };
            char      ioffset_str[32]                 = { 0 };

            while (attr != NULL) {
                if (attr->type == XML_ELEMENT_NODE) {
                    char *content = (char *) xmlNodeGetContent(attr);
                    if (strcmp((const char *) attr->name, "Name") == 0) {
                        char *name_static = (char *) xmlGetProp(
                            attr, (const xmlChar *) "Static");
                        char *name_task = (char *) xmlGetProp(
                            attr, (const xmlChar *) "TaskPrio");

                        snprintf(static_name, sizeof(static_name), "%s_%s",
                                 name_static, name_task);
                        strncpy(tpy_var.name, content,
                                sizeof(tpy_var.name) - 1);
                        xmlFree(name_static);
                        xmlFree(name_task);
                    } else if (strcmp((const char *) attr->name, "Type") == 0) {
                        strncpy(tpy_var.type, content,
                                sizeof(tpy_var.type) - 1);
                    } else if (strcmp((const char *) attr->name, "IGroup") ==
                               0) {
                        strncpy(igroup_str, content, sizeof(igroup_str) - 1);
                        tpy_var.igroup = atoll(igroup_str);
                    } else if (strcmp((const char *) attr->name, "IOffset") ==
                               0) {
                        strncpy(ioffset_str, content, sizeof(ioffset_str) - 1);
                        tpy_var.ioffset = atoll(ioffset_str);
                    }

                    if (content != NULL) {
                        xmlFree(content);
                    }
                }
                attr = attr->next;
            }

            if (strlen(tpy_var.name) > 0 && strlen(tpy_var.type) > 0) {
                int found = 0;
                for (int i = 0; i < tpy->n_statics; i++) {
                    if (strcmp(tpy->statics[i].name, static_name) == 0) {
                        found = 1;
                        tpy->statics[i].n_vars += 1;
                        tpy->statics[i].vars =
                            realloc(tpy->statics[i].vars,
                                    tpy->statics[i].n_vars * sizeof(tpy_var_t));
                        tpy->statics[i].vars[tpy->statics[i].n_vars - 1] =
                            tpy_var;
                        break;
                    }
                }

                if (!found) {
                    tpy->n_statics += 1;
                    tpy->statics = realloc(
                        tpy->statics, tpy->n_statics * sizeof(tpy_static_t));
                    tpy->statics[tpy->n_statics - 1].n_vars = 1;
                    strncpy(tpy->statics[tpy->n_statics - 1].name, static_name,
                            sizeof(tpy->statics[tpy->n_statics - 1].name) - 1);
                    tpy->statics[tpy->n_statics - 1].vars =
                        malloc(sizeof(tpy_var_t));
                    tpy->statics[tpy->n_statics - 1].vars[0] = tpy_var;
                }
            }
        }

        var = var->next;
    }

    return 0;
}

static neu_type_e to_neu_type(char *type)
{
    if (strcmp(type, "BOOL") == 0) {
        return NEU_TYPE_BOOL;
    } else if (strcmp(type, "WORD") == 0) {
        return NEU_TYPE_UINT16;
    } else if (strcmp(type, "INT") == 0) {
        return NEU_TYPE_INT16;
    } else if (strcmp(type, "USINT") == 0) {
        return NEU_TYPE_UINT8;
    } else if (strcmp(type, "BYTE") == 0) {
        return NEU_TYPE_UINT8;
    } else if (strcmp(type, "SINT") == 0) {
        return NEU_TYPE_INT8;
    } else if (strcmp(type, "UINT") == 0) {
        return NEU_TYPE_UINT16;
    } else if (strcmp(type, "UDINT") == 0) {
        return NEU_TYPE_UINT32;
    } else if (strcmp(type, "DINT") == 0) {
        return NEU_TYPE_INT32;
    } else if (strcmp(type, "DWORD") == 0) {
        return NEU_TYPE_UINT32;
    } else if (strcmp(type, "REAL") == 0) {
        return NEU_TYPE_FLOAT;
    } else if (strcmp(type, "LREAL") == 0) {
        return NEU_TYPE_DOUBLE;
    } else {
        return NEU_TYPE_UINT8;
    }
}

void neu_tpy_to_msg(char *driver, tpy_t *tpy, neu_req_add_gtag_t *cmd)
{
    strcpy(cmd->driver, driver);
    cmd->n_group = tpy->n_statics;
    cmd->groups  = calloc(cmd->n_group, sizeof(neu_gdatatag_t));

    for (int i = 0; i < cmd->n_group; i++) {
        strcpy(cmd->groups[i].group, tpy->statics[i].name);
        cmd->groups[i].interval = 1000;
        cmd->groups[i].n_tag    = tpy->statics[i].n_vars;
        cmd->groups[i].tags =
            calloc(cmd->groups[i].n_tag, sizeof(neu_datatag_t));
        for (int k = 0; k < cmd->groups[i].n_tag; k++) {
            char address[128]           = { 0 };
            cmd->groups[i].tags[k].name = strdup(tpy->statics[i].vars[k].name);
            cmd->groups[i].tags[k].type =
                to_neu_type(tpy->statics[i].vars[k].type);
            cmd->groups[i].tags[k].attribute =
                NEU_ATTRIBUTE_READ | NEU_ATTRIBUTE_WRITE;
            snprintf(address, sizeof(address) - 1, "%ld,%ld",
                     tpy->statics[i].vars[k].igroup,
                     tpy->statics[i].vars[k].ioffset);
            cmd->groups[i].tags[k].address     = strdup(address);
            cmd->groups[i].tags[k].description = strdup("");
        }
    }
}